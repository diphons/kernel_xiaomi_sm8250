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

#ifndef _KJSTRING_H
#define _KJSTRING_H

/* 
 * This library could be useful to use both in kernel and 
 * user space. KJSON will only use the Kernel space version.
 */
#ifdef __KERNEL__

#include <linux/string.h>
#include <linux/slab.h>

#define kjstring_allocator(size) kmalloc(size, GFP_KERNEL)
#define kjstring_free(str) kfree(str)

#else

#include <stdlib.h>
#include <string.h>

#define kjstring_allocator(size) malloc(size)
#define kjstring_free(str) free(str)

#endif

struct kjstring_t {
    size_t buffer_size;
    size_t off;

    /*
     * Data could be allocated in another place or embedded in the current struct.
     * In this last case, str_data points to __data (so it points the field just after it self)
     */
	union {
	    char *str_data;
	    const char *c_str_data;
    };
    char __data[];
} __no_randomize_layout;

struct kjstring_iterator {
    const struct kjstring_t *str;
    size_t pos;
};

#define kjstring_str(str) (str)->str_data
#define kjstring_size(str) (str)->off

#define INTOS_SIZE  16  // Size of buffer for integer to string convertion

/*
 * This will create a kjstring_t in the current stack.
 * This function is useful if you need to allocate a short string (less than a stack size)
 * and you don't want to create a new heap area to contain it.
 * @name: The name of your string object.
 * @size: The max size of your string object including null terminator.
 */
#define kjstring_static_declare(name, size)                                 \
    struct kjstring_static_##name {                                         \
        size_t buffer_size;                                                 \
        size_t off;                                                         \
        char *str_data;                                                     \
        char __data[size];                                                  \
    } _##name;                                                              \
    struct kjstring_t *name = (struct kjstring_t*)&_##name;                 \
    name->buffer_size = size; name->off = 0; name->str_data = name->__data; \
    memset(name->str_data, 0x0, size)

/*
 * This will create a kjstring_t object starting from the buffer string pointer.
 * If you just have a string buffer you can create a kjstring_t object to rappresent your string.
 * Be careful, if the kjstring_t object passed as first parameter already points to a buffer, it will be lost. 
 * @str: An empty preallocated kjstring_t struct (on your stack or heap).
 * @buffer: Your string buffer.
 * @buffer_size: Your string buffer size including null terminator.
 * Return: Return the same address of str.
 */
static inline struct kjstring_t* kjstring_new_string_buffer(struct kjstring_t *str, char *buffer, size_t buffer_size)
{
    if(!str || !buffer)
        return NULL;

    str->buffer_size = buffer_size;
    // If the string is empty, off will be 0. Otherwise it will be the last character before 0
    str->off = strnlen(buffer, buffer_size - 1);
    str->str_data = buffer;

    return str;
}

/*
 * This will allocate a kjstring_t object on the heap.
 * @size: The max size of your string.
 * Return: The kjstring_t object.
 * In this case, you have to call kjstring_free to dealloc the string object.
 */
static inline struct kjstring_t *kjstring_alloc(size_t default_size)
{
    struct kjstring_t *str;

    if(!default_size || (str = kjstring_allocator(sizeof(struct kjstring_t) + default_size)) == NULL)
        return NULL;

    memset(str, 0, sizeof(struct kjstring_t) + default_size);
    str->buffer_size = default_size;
    str->str_data = str->__data;

    return str;
}

/*
 * The string will be cleared
 */
#define kjstring_clear(str) do  {                   \
    str->off = 0;                                   \
    memset(str->str_data, 0x0, str->buffer_size);   \
} while(0)

#define kjstring_append_pos(str, src, pos) do {                                 \
    if(pos < str->buffer_size) {                                                \
        size_t len = strlen(src);                                               \
        strncpy(&str->str_data[pos], src, (str->buffer_size - pos));            \
        if((str->buffer_size - pos) <= len) {                                   \
            str->off = str->buffer_size - 1;                                    \
            str->str_data[str->off] = '\0';                                     \
        }                                                                       \
        else {                                                                  \
            str->off = pos + len;                                               \
        }                                                                       \
    }                                                                           \
} while(0)

/*
 * Append a string starting to a specific position
 * @str: kjstring_t object.
 * @src: The pointer to the string to append.
 * @pos: The position to start the append
 */
static inline void kjstring_append_string(struct kjstring_t *str, char *src, size_t pos) {
    kjstring_append_pos(str, src, pos);
}

/*
 * Append an integer starting to a specific position. The integer value will be convert to an ASCII type
 * @str: kjstring_t object.
 * @val: The pointer to the integer to append.
 * @pos: The position to start the append
 */
static inline void kjstring_append_integer(struct kjstring_t *str, int64_t val, size_t pos) {
    char integer[INTOS_SIZE];

    memset(integer, 0, INTOS_SIZE);
    snprintf(integer, INTOS_SIZE, "%lld", val);
    kjstring_append_pos(str, integer, pos);
}

// Type to put on the string is not recognized
static inline void __kjstring_no_append(struct kjstring_t *str, int64_t val, size_t pos) {}

#define kjstring_insert_type(str, src, pos) _Generic((src), \
    char*: kjstring_append_string,                          \
    const char*: kjstring_append_string,                    \
    int64_t: kjstring_append_integer,                       \
    default: __kjstring_no_append                           \
    )(str, src, pos)

/*
 * Use this to append a string or integer type to your kjstring_t object.
 * @str: kjstring_t object.
 * @src: A string or integer value to append
 */
#define kjstring_append(str, src) kjstring_insert_type(str, src, str->off)

/*
 * Use this to truncate the current kjstring_t and add a string or integer type to your kjstring_t object.
 * @str: kjstring_t object.
 * @src: A string or integer value to append
 */
#define kjstring_trunc(str, src) kjstring_insert_type(str, src, 0)

/*
 * Push a char value to the kjstring_t object
 * @str: kjstring_t object.
 * @chr: A cahr value
 */
#define kjstring_push(str, chr) do {                    \
    if(str->off < str->buffer_size - 1)                 \
        str->str_data[str->off++] = chr;                \
} while(0)

/*
 * Initialize an iterator.
 * @str: A non empty kjstring_t object.
 * @iterator: A preallocated struct kjstring_iterator (usually on your stack)
 */
static inline void kjstring_interator_init(const struct kjstring_t *str, struct kjstring_iterator *iterator)
{
    iterator->str = str;
    iterator->pos = 0;
}

/*
 * Declare an iterator on the stack directly using a const char* or char* null term string.
 * Useful if you have a char* pointer to a string and you want a fast secure way to iter on it.
 * @iter_name: The name of the iterator to declare
 * @char_str: A const char* or char* string pointer
 */
#define kjstring_iterator_from_string(iter_name, char_str)			\
	const struct kjstring_t __kj_##iter_name = { 					\
		.buffer_size = strlen(char_str) + 1,						\
		.off = strlen(char_str),									\
		.c_str_data = char_str,										\
	};																\
	struct kjstring_iterator iter_name = {							\
		.str = &__kj_##iter_name,									\
		.pos = 0,													\
	};																\


/*
 * Reset the iterator. It will start from the position 0
 */
#define kjstring_iterator_reset(iterator) (iterator)->pos = 0

/*
 * Create a copy of the iterator in the iterator_dest. 
 */
#define kjstring_copy_iterator(iterator_src, iterator_dest) do {    \
    (iterator_dest)->str = (iterator_src)->str;                     \
    (iterator_dest)->pos = (iterator_src)->pos;                     \
} while(0)

/*
 * Return the current char pointed by the iterator and increment the iterator counter.
 * @iterator: A kjstring_iterator type.
 * @chr: An empty char.
 */
#define kjstring_iterator_next(iterator, chr) do {          \
    chr = '\0';                                             \
    if((iterator)->pos < (iterator)->str->off)              \
        chr = (iterator)->str->c_str_data[(iterator)->pos++]; \
} while(0)

/*
 * Return the current char pointed by the iterator.
 */
static inline char kjstring_iterator_get(struct kjstring_iterator *iterator)
{
    char ret = '\0';

    if(!iterator && !iterator->str)
        return ret;

    if(iterator->pos < iterator->str->off)
        ret = iterator->str->c_str_data[iterator->pos];

    return ret;
}

/*
 * Return true if the iterator point to the end
 */
static inline bool kjstring_iterator_end(struct kjstring_iterator *iterator)
{
    return iterator->pos == iterator->str->off;
}

static inline const char *kjstring_iterator_follow(const struct kjstring_iterator *iterator)
{
	return &iterator->str->c_str_data[iterator->pos];
}

/*
 * Iter all the kjstring_t object.
 * @chr: An empty char type where the next value will be stored
 */
#define kjstring_for_each(iterator, chr)                    \
    for(chr = (iterator)->str->c_str_data[(iterator)->pos] ;  \
        (iterator)->pos < (iterator)->str->off ;            \
        chr = (iterator)->str->c_str_data[++(iterator)->pos])

#endif
