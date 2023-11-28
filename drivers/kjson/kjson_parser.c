// SPDX-License-Identifier: GPL-2.0

/* This file is part of JsonOnKernel.
 *
 * JsonOnKernel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * JsonOnKernel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nome-Programma.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2023, Emanuele Santini
 *
 * Authors: Emanuele Santini <emanuele.santini.88@gmail.com>
*/

#include <linux/module.h>
#include <kjson/kstrtox.h>
#include <kjson/kjson.h>

#define pop_nospace(iterator, chr) do {         \
    do {                                        \
        kjstring_iterator_next(iterator, chr);  \
    } while(chr == 0x20 || chr == 0xa);         \
} while(0)

#define get_nospace(iterator, chr) do {         \
    chr = kjstring_iterator_get(iterator);      \
    while(chr == 0x20 || chr == 0xa) {			\
        kjstring_iterator_next(iterator, chr);  \
        chr = kjstring_iterator_get(iterator);  \
    }                                           \
} while(0)

static struct kjson_container *kjson_start_parser(struct kjstring_iterator *iterator, bool is_nested);
static struct kjson_object_t *kjson_parse_object(struct kjstring_iterator *iterator);

/* Parse the key from the iterator of the json string
 * Returns 0 in case of success
 */
static int parse_key(struct kjstring_iterator *iterator, struct kjstring_t *ret_key);
static int parse_integer(struct kjstring_iterator *iterator, int64_t *ret_value);
static int parse_string(struct kjstring_iterator *iterator, struct kjstring_t *ret_str);

static struct kjson_object_t *parse_value(struct kjstring_iterator *iterator);

static struct kjson_object_t *parse_integer_object(struct kjstring_iterator *iterator);
static struct kjson_object_t *parse_string_object(struct kjstring_iterator *iterator);
static struct kjson_object_t *parse_ctn_object(struct kjstring_iterator *iterator);

static struct kjson_object_t *parse_array_object(struct kjstring_iterator *iterator);
static struct kjson_object_t *parse_string_array(struct kjstring_iterator *iterator);
static struct kjson_object_t *parse_integer_array(struct kjstring_iterator *iterator);
static struct kjson_object_t *parse_ctn_array(struct kjstring_iterator *iterator);
static struct kjson_object_t *parse_null_bool_object(struct kjstring_iterator *iterator);

static size_t find_string_size(struct kjstring_iterator *iterator);
static size_t find_array_size(struct kjstring_iterator *iterator);
static size_t find_ctn_array_size(struct kjstring_iterator *iterator);

struct kjstring_parser_error kjson_parser_error = {
	.buffer_size = KJSTRING_PARSER_ERR_MSG_SIZE,
	.off = 0,
	.str_data = NULL
};

static inline void set_error(char *msg, int64_t pos, char character) 
{
	kjstring_append(((struct kjstring_t*)&kjson_parser_error), msg);
	kjstring_append(((struct kjstring_t*)&kjson_parser_error), ". Position: ");
	kjstring_append(((struct kjstring_t*)&kjson_parser_error), pos);
	kjstring_append(((struct kjstring_t*)&kjson_parser_error), ", Character: ");
	// For a char type kjstring_push is more efficent
	kjstring_push(((struct kjstring_t*)&kjson_parser_error), character);
	
	printk("%s %lld %c\n", msg, pos, character);
}

int parse_integer(struct kjstring_iterator *iterator, int64_t *ret_value)
{
    char nextchar;
    kjstring_static_declare(string_integer, 64);

    if(!ret_value)
    	return 1;

    get_nospace(iterator, nextchar);
    if(nextchar < 0x30 || nextchar > 0x39)
    {
	    set_error("Invalid integer character", iterator->pos, nextchar);
        return 1;
    }

    do
    {
        kjstring_push(string_integer, nextchar);
        kjstring_iterator_next(iterator, nextchar);
        nextchar = kjstring_iterator_get(iterator);
    } while(nextchar >= 0x30 && nextchar <= 0x39);

    if(nextchar == '\0')
    {
	    set_error("Syntax error", iterator->pos, nextchar);
        return 1;
    }

    // An integer shall be terminated by one of this char's: ' ' ',' ']'
    if(nextchar != ' ' && nextchar != ',' && nextchar != ']' && nextchar != '}')
    {
	    set_error("Syntax error", iterator->pos, nextchar);
        return 1;
    }

    if(kstrtol(kjstring_str(string_integer), 10, (long*)ret_value))
    {
    	set_error("Integer error", iterator->pos, nextchar);
        return 1;
  	}

    return 0;
}

int parse_string(struct kjstring_iterator *iterator, struct kjstring_t *ret_str)
{
    char nextchar;

    pop_nospace(iterator, nextchar);    
    if(nextchar != '\"') {
	    set_error("Character not recognized - do you miss '\"' ?", iterator->pos, nextchar);
        return 1;
    }

    kjstring_iterator_next(iterator, nextchar);
    while(nextchar != '\"' && nextchar != '\0')
    {
        kjstring_push(ret_str, nextchar);
        kjstring_iterator_next(iterator, nextchar);
    }

    if(nextchar == '\0')
    {
	    set_error("Syntax error", iterator->pos, nextchar);
    	return 1;
    }

    return 0;
}

size_t find_string_size(struct kjstring_iterator *iterator)
{
    size_t size = 0;
    struct kjstring_iterator iter_cpy;
    char nextchar;

    kjstring_copy_iterator(iterator, &iter_cpy);

    pop_nospace(&iter_cpy, nextchar);
    if(nextchar != '\"') 
    {
	    set_error("Character not recognized - do you miss '\"' ?", iterator->pos, nextchar);
        return 0;
    }

    kjstring_iterator_next(&iter_cpy, nextchar);
    while(nextchar != '\"' && nextchar != '\0')
    {
        size++;
        kjstring_iterator_next(&iter_cpy, nextchar);
    }

    if(nextchar == '\0')
    {
	    set_error("Syntax error", iterator->pos, nextchar);
        return 0;
    }

    return size;
}

struct kjson_object_t *parse_integer_object(struct kjstring_iterator *iterator)
{
    struct kjson_object_t *obj;

    if(kj_alloc(obj, sizeof(int64_t)) == NULL)
    {
	    set_error("Unable to alloc memory", iterator->pos, '\0');
        return NULL;
    }

    if(parse_integer(iterator, (int64_t*)obj->data))
    {
        kj_dealloc(obj);
        return NULL;
    }

    obj->type = KOBJECT_TYPE_INTEGER;

    return obj;
}

struct kjson_object_t *parse_string_object(struct kjstring_iterator *iterator)
{
    struct kjson_object_t *obj;
    struct kjstring_t tmp_string;
    size_t obj_size;

    if(!iterator)
    	return NULL;

    obj_size = find_string_size(iterator) + 1;
    if(kj_alloc(obj, obj_size) == NULL)
    {
    	set_error("Unable to alloc memory", iterator->pos, '\0');
        return NULL;
    }

    kjstring_new_string_buffer(&tmp_string, obj->data, obj_size);

    if(parse_string(iterator, &tmp_string))
    {
        kj_dealloc(obj);
        return NULL;
    }

    obj->type = KOBJECT_TYPE_STRING;

    return obj;
}

struct kjson_object_t *parse_ctn_object(struct kjstring_iterator *iterator)
{
    struct kjson_object_t *obj;
    struct kjson_container *nest;

    if((nest = kjson_start_parser(iterator, true)) == NULL)
    	return NULL;

    if(kj_alloc(obj, sizeof(struct kjson_container*)) == NULL)
    {
	    set_error("Unable to alloc memory", iterator->pos, '\0');
        return NULL;
    }

    obj->type = KOBJECT_TYPE_OBJECT;
    *(struct kjson_container**)obj->data = (struct kjson_container*)nest;

    return obj;
}

struct kjson_object_t *parse_array_object(struct kjstring_iterator *iterator)
{
    struct kjson_object_t *obj;
    char nextchar;

    if(!iterator)
    	return NULL;

    pop_nospace(iterator, nextchar);
    if(nextchar != '[')
    {
	    set_error("Invalid operand on array of objects", iterator->pos, nextchar);
        return NULL;
    }

    get_nospace(iterator, nextchar);

    switch(nextchar)
    {
        // Check if array is empty
        case ']':
        {
            pop_nospace(iterator, nextchar);

            // Return empty array
            if(kj_alloc(obj, sizeof(struct kjson_array_struct)) == NULL)
            {
            	set_error("Unable to alloc memory", iterator->pos, '\0');
                return NULL;
        	}

            // We consider an empty array as an integer empty array
            obj->type = KOBJECT_TYPE_INTEGER_ARRAY;
            ((struct kjson_array_struct*)obj->data)->len = 0;

            break;
        }
        case '\"':
            obj = parse_string_array(iterator);
            break;
        case '{':
            obj = parse_ctn_array(iterator);
            break;
        default:
            obj = parse_integer_array(iterator);
    }

    return obj;
}

size_t find_array_size(struct kjstring_iterator *iterator)
{
    struct kjstring_iterator itra;
    size_t array_size;
    char nextchar;

    kjstring_copy_iterator(iterator, &itra);
    array_size = 1;

    do
    {
        pop_nospace(&itra, nextchar);
        if(nextchar == ',')
        	array_size++;
    }
    while(nextchar != ']' && nextchar != '\0');

    if(nextchar == '\0')
    {
	    set_error("Syntax error", iterator->pos, nextchar);
        return 0;
    }

    return array_size;
}

size_t find_ctn_array_size(struct kjstring_iterator *iterator)
{
    struct kjstring_iterator itra;
    size_t array_size;
    char nextchar;

    kjstring_copy_iterator(iterator, &itra);
    array_size = 1;

    do
    {
        pop_nospace(&itra, nextchar);
        if(nextchar == '}')
        {
            pop_nospace(&itra, nextchar);
            if(nextchar == ',')
            	array_size++;
            else
            	break;
        }
    }
    while(nextchar != '\0');

    if(nextchar == '\0')
    {
	    set_error("Syntax error", iterator->pos, nextchar);
        return 0;
    }

    return array_size;
}

struct kjson_object_t *parse_string_array(struct kjstring_iterator *iterator)
{
    struct kjson_object_t *obj;
    struct kjson_array_struct *data_hdr;
    char **parsed_str_buffer;
    struct kjstring_t parsed_str;
    unsigned int array_size, str_size, i, j;
    char nextchar;

    if(!iterator)
        return NULL;

	// get the json array size - iterator will not change it's position after this call
    array_size = find_array_size(iterator);
    // if find_array_size is zero the buffer reach the end (no json array detected)
    if(array_size == 0)
        return NULL;

    if(kj_alloc(obj, (array_size * sizeof(char*)) + sizeof(struct kjson_array_struct)) == NULL)
    {
    	set_error("Unable to alloc memory", iterator->pos, '\0');
        return NULL;
    }

    obj->type = KOBJECT_TYPE_STRING_ARRAY;
    data_hdr = (struct kjson_array_struct*)obj->data;
    data_hdr->len = array_size;
    parsed_str_buffer = (char**)(data_hdr->data);

    for(i = 0; i < array_size; i++)
    {
        str_size = find_string_size(iterator) + 1; // Last 1 byte for Null terminator

        // alloc the buffer for the string to insert into the json container
        if((parsed_str_buffer[i] = kzalloc(str_size, GFP_KERNEL)) == NULL)
        {
        	set_error("Unable to alloc memory", iterator->pos, '\0');
            goto FAIL;
        }

        kjstring_new_string_buffer(&parsed_str, parsed_str_buffer[i], str_size);

        if(parse_string(iterator, &parsed_str))
            goto FAIL; // TODO the last parsed_str_buffer allocated will not release - improve the deallocator

        pop_nospace(iterator, nextchar);
        if(nextchar != ',')
        {
            if(nextchar == ']' && i == array_size - 1)
                goto OUT;
            else
            {
                goto FAIL;
            }
        }
    }

    // This point it shouldn't reached
    goto FAIL;

FAIL:
    for(j = 0; j < i; j++)
        kfree(parsed_str_buffer[j]);

    if(parsed_str_buffer[i] != NULL)
        kfree(parsed_str_buffer[j]);

    kj_dealloc(obj);
    obj = NULL;
OUT:
    return obj;
}

struct kjson_object_t *parse_integer_array(struct kjstring_iterator *iterator)
{
    struct kjson_object_t *obj;
    struct kjson_array_struct *data_hdr;
    unsigned int array_size, i;
    char nextchar;

    if(!iterator)
        return NULL;

    array_size = find_array_size(iterator);
    // if find_array_size is zero the buffer reach the end
    if(array_size == 0)
        return NULL;

    if(kj_alloc(obj, (array_size * sizeof(int64_t)) + sizeof(struct kjson_array_struct)) == NULL)
        return NULL;

    obj->type = KOBJECT_TYPE_INTEGER_ARRAY;
    data_hdr = (struct kjson_array_struct*)obj->data;
    data_hdr->len = array_size;

    for(i = 0; i < array_size; i++)
    {
        if(parse_integer(iterator, (int64_t*)(data_hdr->data + (i * sizeof(int64_t)))))
            goto FAIL;

        pop_nospace(iterator, nextchar);
        if(nextchar != ',')
        {
            if(nextchar == ']' && i == array_size - 1)
                goto OUT;
            else
                goto FAIL;
        }
    }

    // This point it shouldn't reached
    goto FAIL;

FAIL:

    kj_dealloc(obj);
    obj = NULL;
OUT:
    return obj;
}

struct kjson_object_t *parse_ctn_array(struct kjstring_iterator *iterator)
{
    struct kjson_object_t *obj;
    struct kjson_array_struct *data_hdr;
    struct kjson_container **parsed_obj;
    unsigned int array_size, i, j;
    char nextchar;

    if(!iterator)
        return NULL;

    array_size = find_ctn_array_size(iterator);

    // if find_ctn_array_size is zero the buffer reach the end
    if(array_size == 0)
        return NULL;

    if(kj_alloc(obj, (array_size * sizeof(struct kjson_container*)) + sizeof(struct kjson_array_struct)) == NULL)
        return NULL;

    obj->type = KOBJECT_TYPE_OBJECT_ARRAY;
    data_hdr = (struct kjson_array_struct*)obj->data;
    data_hdr->len = array_size;
    parsed_obj = (struct kjson_container**)(data_hdr->data);

    for(i = 0; i < array_size; i++)
    {
        if((parsed_obj[i] = kjson_start_parser(iterator, true)) == NULL)
            goto FAIL;

        pop_nospace(iterator, nextchar);
        if(nextchar != ',')
        {
            if(nextchar == ']' && i == array_size - 1)
                goto OUT;
            else
                goto FAIL;
        }
    }

    // This point it shouldn't reached
    goto FAIL;

FAIL:

    for(j = 0; j < i; j++)
        kjson_delete_container(parsed_obj[j]);

    if(parsed_obj[i] != NULL)
        kjson_delete_container(parsed_obj[j]);

    kj_dealloc(obj);
    obj = NULL;
OUT:
    return obj;
}

struct kjson_object_t *parse_null_bool_object(struct kjstring_iterator *iterator)
{
	struct kjson_object_t *obj = NULL;
	const char *next_str = kjstring_iterator_follow(iterator);

	if(!iterator || !iterator->str)
		return NULL;


	if(!strncmp(next_str, "null", 4) || !strncmp(next_str, "NULL", 4))
	{
		if(kj_alloc(obj, 0) == NULL)
			return NULL;

		obj->type = KOBJECT_TYPE_OBJECT_NULL;
		
		iterator->pos += 4;
	}
	else if(!strncmp(next_str, "true", 4) || !strncmp(next_str, "TRUE", 4))
	{
		if(kj_alloc(obj, sizeof(int)) == NULL)
			return NULL;

		obj->type = KOBJECT_TYPE_OBJECT_BOOL;
		*(int*)obj->data = 1;

		iterator->pos += 4;
	}
	else if(!strncmp(next_str, "false", 5) || !strncmp(next_str, "FALSE", 5))
	{
		if(kj_alloc(obj, sizeof(int)) == NULL)
			return NULL;

		obj->type = KOBJECT_TYPE_OBJECT_BOOL;
		*(int*)obj->data = 0;

		iterator->pos += 5;
	}

	return obj;
}

struct kjson_object_t *parse_value(struct kjstring_iterator *iterator)
{
    struct kjson_object_t *obj;
    char nextchar;

    get_nospace(iterator, nextchar);

    switch(nextchar)
    {
        case '"':
            obj = parse_string_object(iterator);
            break;
        case '[':
            obj = parse_array_object(iterator);
            break;
        case '{':
            obj = parse_ctn_object(iterator);
            break;
        case 'n':
        case 'N':
        case 'f':
        case 'F':
        case 't':
        case 'T':
            obj = parse_null_bool_object(iterator);
            break;
        default:
            obj = parse_integer_object(iterator);
    }

    return obj;
}

int parse_key(struct kjstring_iterator *iterator, struct kjstring_t *ret_key)
{
    return parse_string(iterator, ret_key);
}

struct kjson_object_t *kjson_parse_object(struct kjstring_iterator *iterator)
{
    struct kjson_object_t *obj;
    char nextchar;
    kjstring_static_declare(key, KJSON_KEY_SIZE);

    /* Parse key */
    if(parse_key(iterator, key))
        return NULL;

    /* Two points after key */
    pop_nospace(iterator, nextchar);
    if(nextchar != ':')
    {
	    set_error("Character not recognized - do you miss ':' ?", iterator->pos, nextchar);
        return NULL;
    }

    /* Parse value and create kjson_object_t */
    if((obj = parse_value(iterator)) == NULL)
        return NULL;

    strncpy(obj->key, kjstring_str(key), KJSON_KEY_SIZE);

    return obj;
}

struct kjson_container *kjson_start_parser(struct kjstring_iterator *iterator, bool is_nested)
{
    struct kjson_container *ctn;
    struct kjson_object_t *obj;
    char nextchar, nextchar_t;

    // Json script starts with {
    pop_nospace(iterator, nextchar);

    if(nextchar != '{')
    {
    	set_error("No starting scoope found", iterator->pos, nextchar);
        return NULL;
    }

    if(IS_ERR(ctn = kjson_new_container()))
    {
	    set_error("Unable to create a new kjson container", iterator->pos, nextchar);
        return NULL;
    }

    // If is empty ?
    get_nospace(iterator, nextchar_t);
    if(nextchar_t == '}')
    {
        pop_nospace(iterator, nextchar);
        get_nospace(iterator, nextchar);

        // nested json cannot terminate with \0 just after the }
        if(!is_nested && nextchar != '\0')
        {
        	set_error("Json not recognized - syntax error", iterator->pos, nextchar);
            return NULL;
        }
        else
            return ctn;
    }

    while(nextchar != '}' && nextchar != '\0')
    {
        if((obj = kjson_parse_object(iterator)) == NULL) {
            goto FAIL;
        }

        if(__kjson_push_object(ctn, obj))
        {
        	set_error("Unable to push a new object to the kjson container", iterator->pos, nextchar);        	
            kjson_delete_object(obj);
            goto FAIL;
        }

        pop_nospace(iterator, nextchar);
    }

    if(nextchar == '\0')
    {
    	set_error("Json ends without anything to parse", iterator->pos, nextchar); 
        goto FAIL;
    }

    // If this is not a nested json, we don't have anymore charachter after the last scope '}'
    if(!is_nested)
    {
        pop_nospace(iterator, nextchar);
        if(nextchar != '\0')
        {
	        set_error("Json contains strange characters after the end scoope", iterator->pos, nextchar);
            goto FAIL;
        }
    }

    goto OUT;

FAIL:
    kjson_delete_container(ctn);
    ctn = NULL;

OUT:
    return ctn;
}

struct kjson_container *kjson_parse(const char *json_str)
{
	// Inizialize the memory for errors message
	kjson_parser_error.str_data = kjson_parser_error.__data;
	kjstring_clear(((struct kjstring_t*)&kjson_parser_error));
	
	kjstring_iterator_from_string(iterator, json_str);
    return kjson_start_parser(&iterator, false);
}
EXPORT_SYMBOL_GPL(kjson_parse);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Emanuele Santini <emanuele.santini.88@gmail.com>");
