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
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/hashtable.h>
#include <linux/jhash.h>
#include <linux/err.h>
#include <kjson/kjson.h>

// Compress u32 hash number to the correct index for obj_table_root map array
#define kj_hash_compressor(hash) (hash >> (32 - KJSON_BITS_SIZE))

// Get the obj_table_root index
#define kj_get_table_index(key) kj_hash_compressor(jhash(key, strlen(key), 0))

struct kjson_container {
    DECLARE_HASHTABLE(obj_table_root, KJSON_BITS_SIZE);
};

static struct kjson_object_t* __kj_create_integer(void *data, size_t data_len);
static struct kjson_object_t* __kjson_create_string(void *data, size_t data_len);
static struct kjson_object_t* __kjson_create_ctn_object(void *data, size_t data_len);
static struct kjson_object_t* __kjson_create_integer_array(void *data, size_t data_len);
static struct kjson_object_t* __kjson_create_string_array(void *data, size_t data_len);
static struct kjson_object_t* __kjson_create_ctn_object_array(void *data, size_t data_len);
static struct kjson_object_t* __kjson_create_null(void *data, size_t data_len);
static struct kjson_object_t* __kjson_create_bool(void *data, size_t data_len);

typedef struct kjson_object_t* (*kjson_create_ops_t)(void*, size_t);

static int kjson_dump_process_container(struct kjson_container *ctn, struct kjstring_t *json_dmp);
static int kjson_dump_object(struct kjson_object_t *obj, struct kjstring_t *json_dmp);

static void __kjson_dump_integer(struct kjson_object_t *obj, struct kjstring_t *json_dmp);
static void __kjson_dump_string(struct kjson_object_t *obj, struct kjstring_t *json_dmp);
static void __kjson_dump_ctn_object(struct kjson_object_t *obj, struct kjstring_t *json_dmp);
static void __kjson_dump_integer_array(struct kjson_object_t *obj, struct kjstring_t *json_dmp);
static void __kjson_dump_string_array(struct kjson_object_t *obj, struct kjstring_t *json_dmp);
static void __kjson_dump_ctn_object_array(struct kjson_object_t *obj, struct kjstring_t *json_dmp);
static void __kjson_dump_null(struct kjson_object_t *obj, struct kjstring_t *json_dmp);
static void __kjson_dump_bool(struct kjson_object_t *obj, struct kjstring_t *json_dmp);

typedef void (*kjson_dump_ops_t)(struct kjson_object_t*, struct kjstring_t*);

static void copy_string_array(struct kjson_object_t *obj, char **str_array, size_t array_len);
static void dealloc_string_array(struct kjson_object_t *obj);
static void dealloc_object_array(struct kjson_object_t *obj);

struct kjson_container *kjson_new_container(void)
{
    struct kjson_container *ctn;

    if(!(ctn = kmalloc(sizeof(struct kjson_container), GFP_KERNEL)))
        return ERR_PTR(-ENOMEM);

    memset(ctn, 0x0, sizeof(struct kjson_container));
    hash_init(ctn->obj_table_root);

    return ctn;
}
EXPORT_SYMBOL_GPL(kjson_new_container);

void kjson_delete_container(struct kjson_container *ctn)
{
    struct kjson_object_t *obj;
    struct hlist_node *tmp;
    unsigned int i = 0;

    if(unlikely(!ctn))
        return;

    hash_for_each_safe(ctn->obj_table_root, i, tmp, obj, obj_list)
    {
        kjson_delete_object(obj);
    }

    kfree(ctn);
}
EXPORT_SYMBOL_GPL(kjson_delete_container);

void kjson_delete_object(struct kjson_object_t *obj)
{
    struct kjson_container *ctn = NULL;

    switch(obj->type)
    {
        case KOBJECT_TYPE_OBJECT:
            ctn = kjson_as_container(obj);
            kjson_delete_container(ctn);
            break;

        case KOBJECT_TYPE_STRING_ARRAY:
            dealloc_string_array(obj);
            break;

        case KOBJECT_TYPE_OBJECT_ARRAY:
            dealloc_object_array(obj);
            break;
        default: break;
    }

    kj_dealloc(obj);
}

void dealloc_string_array(struct kjson_object_t *obj)
{
    size_t array_len;
    char **parr;
    int i;

    parr = kjson_as_string_array(obj);
    array_len = kjson_array_length(obj);

    for(i = 0; i < array_len; i++)
        kfree(parr[i]);
}

void dealloc_object_array(struct kjson_object_t *obj)
{
    size_t array_len;
    struct kjson_container **parr;
    int i;

    parr = kjson_as_container_array(obj);
    array_len = kjson_array_length(obj);

    for(i = 0; i < array_len; i++)
        kjson_delete_container(parr[i]);
}

struct kjson_object_t *kjson_lookup_object(struct kjson_container *ctn, const char *key)
{
    struct kjson_object_t *obj;
    u32 index;

    if(unlikely(!ctn))
        return NULL;

    index = kj_get_table_index(key);

    hash_for_each_possible(ctn->obj_table_root, obj, obj_list, index)
    {
        // Check the collision.
        if(!strcmp(obj->key, key))
            break;
    }

    return obj;
}
EXPORT_SYMBOL_GPL(kjson_lookup_object);

void kjson_pop_object(struct kjson_container *ctn, const char *key)
{
    struct kjson_object_t *obj;

    if(unlikely(!ctn))
        return;

    obj = kjson_lookup_object(ctn, key);
    hash_del(&obj->obj_list);

    kjson_delete_object(obj);
}
EXPORT_SYMBOL(kjson_pop_object);

int __kjson_push_object(struct kjson_container *ctn, struct kjson_object_t *obj)
{
    struct kjson_object_t *tmp;
    u32 index;

    if(unlikely(!ctn || !obj))
        return -EINVAL;

    index = kj_get_table_index(obj->key);

    // Control if the same key already exist
    hash_for_each_possible(ctn->obj_table_root, tmp, obj_list, index)
    {
        if(!strcmp(obj->key, tmp->key)) {
            kj_dealloc(obj);
            return 1;
        }
    }

    hash_add(ctn->obj_table_root, &obj->obj_list, index);

    return 0;
}
EXPORT_SYMBOL_GPL(__kjson_push_object);

void copy_string_array(struct kjson_object_t *obj, char **str_array, size_t array_len)
{
    char **parr;
    size_t str_size;
    int i;

    parr = kjson_as_string_array(obj);

    for(i = 0; i < array_len; i++) {
        str_size = strlen(str_array[i]);
        parr[i] = kmalloc(str_size + 1, GFP_KERNEL);
        strncpy(parr[i], str_array[i], str_size);
        *(parr[i] + str_size) = '\0';
    }
}

struct kjson_object_t *__kj_create_integer(void *data, size_t data_len)
{
    struct kjson_object_t *obj;

    if(kj_alloc(obj, sizeof(int64_t)) == NULL)
        return NULL;

    *(int64_t*)obj->data = *(int64_t*)data;

    return obj;
}

struct kjson_object_t *__kjson_create_string(void *data, size_t data_len)
{
    struct kjson_object_t *obj;

    if(kj_alloc(obj, data_len + 1) == NULL)
        return NULL;

    strncpy(obj->data, data, data_len);
    obj->data[data_len] = '\0';

    return obj;
}

struct kjson_object_t *__kjson_create_ctn_object(void *data, size_t data_len)
{
    struct kjson_container *nest;
    struct kjson_object_t *obj;

    if(kj_alloc(obj, sizeof(struct kjson_container*)) == NULL)
        return NULL;

    nest = (struct kjson_container*)data;
    *(struct kjson_container**)obj->data = (struct kjson_container*)nest;

    return obj;
}

struct kjson_object_t *__kjson_create_integer_array(void *data, size_t data_len)
{
    struct kjson_object_t *obj;
    struct kjson_array_struct *s;

    if(kj_alloc(obj, (data_len * sizeof(int64_t)) + sizeof(struct kjson_array_struct)) == NULL)
        return NULL;

    s = (struct kjson_array_struct*)obj->data;
    s->len = data_len;

    memcpy(s->data, data, data_len * sizeof(int64_t));

    return obj;
}

struct kjson_object_t *__kjson_create_string_array(void *data, size_t data_len)
{
    struct kjson_object_t *obj;
    struct kjson_array_struct *s;

    if(kj_alloc(obj, (data_len * sizeof(char*)) + sizeof(struct kjson_array_struct)) == NULL)
        return NULL;

    s = (struct kjson_array_struct*)obj->data;
    s->len = data_len;
    copy_string_array(obj, data, data_len);

    return obj;
}

struct kjson_object_t *__kjson_create_ctn_object_array(void *data, size_t data_len)
{
    struct kjson_object_t *obj;
    struct kjson_array_struct *s;
    struct kjson_container **nest_arr;
    unsigned int i;

    if(kj_alloc(obj, (data_len * sizeof(struct kjson_container*)) + sizeof(struct kjson_array_struct)) == NULL)
        return NULL;

    s = (struct kjson_array_struct*)obj->data;
    s->len = data_len;

    nest_arr = (struct kjson_container**)data;

    for(i = 0; i < s->len; i++)
        ((struct kjson_container**)s->data)[i] = nest_arr[i];

    return obj;
}

struct kjson_object_t* __kjson_create_null(void *data, size_t data_len)
{
	struct kjson_object_t *obj;
	
    if(kj_alloc(obj, 0) == NULL)
        return NULL;
        
    return obj;
}

struct kjson_object_t* __kjson_create_bool(void *data, size_t data_len)
{
	struct kjson_object_t *obj;
	
    if(kj_alloc(obj, sizeof(int)) == NULL)
        return NULL;
        
    *(int*)obj->data = *(int*)data;

    return obj;
}

// Do not change the sequence of this array
static kjson_create_ops_t kj_create_ops[] = {
    __kj_create_integer,
    __kjson_create_string,
    __kjson_create_ctn_object,
    __kjson_create_integer_array,
    __kjson_create_string_array,
    __kjson_create_ctn_object_array,
    __kjson_create_null,
    __kjson_create_bool
};

// void kjson_push_object(struct kjson_container *json_ctn, kjson_type type, const char *key, void *data, ...)
int kjson_push_object(struct kjson_container *ctn, const char *key, enum kjson_object_type type, void *data, size_t data_len)
{
    struct kjson_object_t *obj;

    if(unlikely(!ctn || !key))
		return -EINVAL;

    if(unlikely(!data && type != KOBJECT_TYPE_OBJECT_NULL))
		return -EINVAL;

    if((int)type > KOBJECT_NUM - 1 || (int)type < 0)
		return -EINVAL;

    if((obj = kj_create_ops[(int)type - 1](data, data_len)) == NULL)
    	return -EINVAL;

    obj->type = type;
    strncpy(obj->key, key, KJSON_KEY_SIZE);

    // If an object with same key already exist, return 1
    if(__kjson_push_object(ctn, obj))
    	return 1;

    return 0;
}
EXPORT_SYMBOL_GPL(kjson_push_object);

#define open_scoope(json_dmp)       kjstring_append(json_dmp, "{")
#define close_scoope(json_dmp)      kjstring_append(json_dmp, "}")
#define open_array(json_dmp)        kjstring_append(json_dmp, "[")
#define close_array(json_dmp)       kjstring_append(json_dmp, "]")

#define set_integer(json_dmp, val) kjstring_append(json_dmp, val)

#define set_string(json_dmp, str) do {  \
    kjstring_append(json_dmp, "\"");    \
    kjstring_append(json_dmp, str);     \
    kjstring_append(json_dmp, "\"");    \
} while(0)

#define set_key(json_dmp, str) do {     \
    set_string(json_dmp, str);          \
    kjstring_append(json_dmp, ": ");    \
} while(0)

#define set_array(json_dmp, array, array_len, set_FOO) do {        \
    int i;                                              \
    open_array(json_dmp);                               \
    for(i = 0; i < array_len; i++) {                    \
        if(i > 0)                                       \
            kjstring_append(json_dmp, ", ");            \
        set_FOO(json_dmp, array[i]);                    \
    }                                                   \
    close_array(json_dmp);                              \
} while(0)

#define set_ctn(json_dmp, ctn) kjson_dump_process_container(ctn, json_dmp)

#define set_integer_array(json_dmp, array_ptr, array_len) set_array(json_dmp, array_ptr, array_len, set_integer)
#define set_string_array(json_dmp, array_ptrs, array_len) set_array(json_dmp, array_ptrs, array_len, set_string)
#define set_ctn_array(json_dmp, array_ptrs, array_len) set_array(json_dmp, array_ptrs, array_len, set_ctn)

// Do not change the sequence of this array
kjson_dump_ops_t kjson_dump_ops[] = {
    __kjson_dump_integer,
    __kjson_dump_string,
    __kjson_dump_ctn_object,
    __kjson_dump_integer_array,
    __kjson_dump_string_array,
    __kjson_dump_ctn_object_array,
    __kjson_dump_null,
    __kjson_dump_bool
};

void __kjson_dump_integer(struct kjson_object_t *obj, struct kjstring_t *json_dmp)
{
    // set key
    set_key(json_dmp, obj->key);
    // set value
    set_integer(json_dmp, kjson_as_integer(obj));
}

void __kjson_dump_string(struct kjson_object_t *obj, struct kjstring_t *json_dmp)
{
    // set key
    set_key(json_dmp, obj->key);
    // set value
    set_string(json_dmp, kjson_as_string(obj));
}

void __kjson_dump_ctn_object(struct kjson_object_t *obj, struct kjstring_t *json_dmp)
{
    struct kjson_container *ctn = kjson_as_container(obj);

    set_key(json_dmp, obj->key);
    set_ctn(json_dmp, ctn);
}

void __kjson_dump_integer_array(struct kjson_object_t *obj, struct kjstring_t *json_dmp)
{
    int64_t *p = kjson_as_integer_array(obj);

    set_key(json_dmp, obj->key);
    set_integer_array(json_dmp, p, kjson_array_length(obj));
}

void __kjson_dump_string_array(struct kjson_object_t *obj, struct kjstring_t *json_dmp)
{
    char **p = kjson_as_string_array(obj);

    set_key(json_dmp, obj->key);
    set_string_array(json_dmp, p, kjson_array_length(obj));
}

void __kjson_dump_ctn_object_array(struct kjson_object_t *obj, struct kjstring_t *json_dmp)
{
    struct kjson_container **ctn = kjson_as_container_array(obj);

    set_key(json_dmp, obj->key);
    set_ctn_array(json_dmp, ctn, kjson_array_length(obj));
}

void __kjson_dump_null(struct kjson_object_t *obj, struct kjstring_t *json_dmp)
{
    set_key(json_dmp, obj->key);
    kjstring_append(json_dmp, "null");
}

void __kjson_dump_bool(struct kjson_object_t *obj, struct kjstring_t *json_dmp)
{
    set_key(json_dmp, obj->key);

    if(kjson_as_bool(obj))
		kjstring_append(json_dmp, "true");
	else
		kjstring_append(json_dmp, "false");
}

int kjson_dump_object(struct kjson_object_t *obj, struct kjstring_t *json_dmp)
{
    if(unlikely(!obj || !json_dmp))
    	return -EINVAL;

    if(obj->type == 0)
    	return -EINVAL;

    kjson_dump_ops[obj->type - 1](obj, json_dmp);

    return 0;
}

int kjson_dump_process_container(struct kjson_container *ctn, struct kjstring_t *json_dmp)
{
    struct kjson_object_t *curr;
    struct hlist_node *tmp;
    unsigned int i = 0, j = 0;
    int ret = 0;

    open_scoope(json_dmp);

    hash_for_each_safe(ctn->obj_table_root, i, tmp, curr, obj_list)
    {
        if(j > 0)
        	kjstring_append(json_dmp, ", ");

        if((ret = kjson_dump_object(curr, json_dmp)))
        	return ret; // PARSING ERROR

        j = 1;
    }

    close_scoope(json_dmp);

    return 0;
}

struct kjstring_t *kjson_dump(struct kjson_container *ctn)
{
    struct kjstring_t *json_dmp = NULL;
    int ret;

    if(unlikely(!ctn))
    	return ERR_PTR(-EINVAL);

    if((json_dmp = kjstring_alloc(KJSON_MEMORY_DUMP_SIZE)) == NULL)
    	return ERR_PTR(-ENOMEM);

    if((ret = kjson_dump_process_container(ctn, json_dmp)))
    {
    	kjstring_free(json_dmp);
    	json_dmp = ERR_PTR(ret);
    }

    return json_dmp;
}
EXPORT_SYMBOL_GPL(kjson_dump);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Emanuele Santini <emanuele.santini.88@gmail.com>");
