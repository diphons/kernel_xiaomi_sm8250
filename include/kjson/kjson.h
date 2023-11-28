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

#ifndef _KJSON_H
#define _KJSON_H

#include <linux/types.h>
#include <kjson/kjstring.h>

#define KJSON_KEY_SIZE  256

#ifndef KJSON_BITS_SIZE
#define KJSON_BITS_SIZE 6
#endif

#ifndef KJSON_MEMORY_DUMP_SIZE
#define KJSON_MEMORY_DUMP_SIZE 4096
#endif

/*
 * This is the kjson data structure.
 */
struct kjson_container;

/*
 * JSON value type supported.
 */
enum kjson_object_type {
    KOBJECT_TYPE_INTEGER        = 1,
    KOBJECT_TYPE_STRING         = 2,
    KOBJECT_TYPE_OBJECT         = 3,
    KOBJECT_TYPE_INTEGER_ARRAY  = 4,
    KOBJECT_TYPE_STRING_ARRAY   = 5,
    KOBJECT_TYPE_OBJECT_ARRAY   = 6,
    KOBJECT_TYPE_OBJECT_NULL	= 7,
    KOBJECT_TYPE_OBJECT_BOOL	= 8,
    KOBJECT_NUM                 = 9
};

/*
 * kjson_object_t rappresent a JSON object into a JSON data structure.
 * Each kjson_object_t is a node of an Hash Table defined into the kjson_container.
 * Key is the JSON key of the object, used to access the Hash Table.
 * data is a variable array that store the JSON value type (int, string, array, ecc).
 */
struct kjson_object_t {
    struct hlist_node obj_list;
    enum kjson_object_type type;
    char key[KJSON_KEY_SIZE];
    char data[];
};

/*
 * If data field on kjson_object_t is an array, it's header is defined by this struct
 */
struct kjson_array_struct {
    size_t len;
    char data[];
};

#define KJSTRING_PARSER_ERR_MSG_SIZE	512
/*
 * KJSON parser error struct - it is a redefinition of the struct kjstring
 */
struct kjstring_parser_error {
    size_t buffer_size;
    size_t off;
	char *str_data;
    char __data[KJSTRING_PARSER_ERR_MSG_SIZE];
} __no_randomize_layout;

extern struct kjstring_parser_error kjson_parser_error;

#define kjson_parser_error_msg	kjson_parser_error.str_data

/*
 * Alloc and Dealloc a kjson_object_t
 */
#define kj_alloc(obj, data_len) (obj = kzalloc(sizeof(struct kjson_object_t) + data_len, GFP_KERNEL))
#define kj_dealloc(obj) kfree(obj)

/*
 * Creates a new JSON container.
 * It returns the kjson_container object pointer, or an error code 
 * in case of error (ENOMEM).
 */
extern struct kjson_container *kjson_new_container(void);

/*
 * Dealloc a JSON container with all sub container.
 * @ctn: Container to dealloc
 */
extern void kjson_delete_container(struct kjson_container *ctn);

/*
 * Get the Json object with specific key.
 * @ctn: A non empty JSON contanier.
 * @key: Key of the object to return.
 * It returns the kjson_object_t that contains the value with the specific key.
 * If no object with this key exist, NULL is returned.
 */
extern struct kjson_object_t *kjson_lookup_object(struct kjson_container *ctn, const char *key);

/*
 * Remove the object with specific key.
 * @ctn: A non empty JSON contanier.
 * @key: Key of the object to delete.
 */
extern void kjson_pop_object(struct kjson_container *ctn, const char *key);

/*
 * Insert an object with the specific key on the ctn container.
 * Used by the utility insert functions below.
 * @ctn: A non empty JSON contanier.
 * @key: Key of the new object.
 * @type: The type of the object. Read the kjson_object_type enum type.
 * @data: A pointer to the relative data (To an integer, a string (char*), another kjson_t or the first element of the array)
 * @data_len: Len of the data: (0 for Integer type or one annidate json container, strlen for string type, array element for array type)
 * In case of success it will return 0.
 */
extern int kjson_push_object(struct kjson_container *ctn, const char *key, enum kjson_object_type type, void *data, size_t data_len);

/*
 * Will push a kjson_object_t type.
 * Used by kjson parser engine.
 * @ctn: A non empty JSON contanier.
 * @obj: The specific kjson_object_t to push.
 */
extern int __kjson_push_object(struct kjson_container *ctn, struct kjson_object_t *obj);

/*
 * Used by the parser. Do not call this function.
 */
extern void kjson_delete_object(struct kjson_object_t *obj);

/* ****** utility insert functions ****** */

/*
 * Insert an integer.
 * Example: kjson_push_integer(ctn, "MyKeyForInteger", 1);
 */
#define kjson_push_integer(ctn, key, T) do {                        \
    int64_t val = (int64_t)T;                                       \
    kjson_push_object(ctn, key, KOBJECT_TYPE_INTEGER, &val, 0);     \
} while(0)

/*
 * Insert a string.
 * Example: kjson_push_string(ctn, "MyKeyForString", "MyString");
 */
#define kjson_push_string(ctn, key, T) do {                         \
    kjson_push_object(ctn, key, KOBJECT_TYPE_STRING, T, strlen(T)); \
} while(0)

/*
 * Insert an annidated JSON. child_ctn will be a child of ctn.
 * child_ctn shall be a type of kjson_container.
 * In case of you'll call kjson_delete_container on ctn, child_ctn will be deleted too.
 */
#define kjson_push_container(ctn, key, child_ctn) do {              \
    kjson_push_object(ctn, key, KOBJECT_TYPE_OBJECT, child_ctn, 0); \
} while(0)

/*
 * Insert an array of integers.
 * Example: kjson_push_integer_array(ctn, "MyKeyForArrayInt", 1, 2, 3, 5);
 */
#define kjson_push_integer_array(ctn, key, ... ) do {                                              \
    int64_t arr[] = { __VA_ARGS__ };                                                               \
    kjson_push_object(ctn, key, KOBJECT_TYPE_INTEGER_ARRAY, arr, sizeof(arr) / sizeof(int64_t));   \
} while(0)

/*
 * Insert an array of strings.
 * Example: kjson_push_string_array(ctn, "MyKeyForArrayChar", "str1", "str2", "str3", "str5");
 */
#define kjson_push_string_array(ctn, key, ... ) do {                                            \
    char *arr[] = { __VA_ARGS__ };                                                              \
    kjson_push_object(ctn, key, KOBJECT_TYPE_STRING_ARRAY, arr, sizeof(arr) / sizeof(char*));   \
} while(0)

/*
 * Insert an annidated JSON array. All kjson_container objects inserts will be childs of ctn.
 * Example: kjson_push_container_array(ctn, "AKeyOf", child1, child2, child3, child4);
 * Where child-n are kjson_container types.
 * In case of you'll call kjson_delete_container on ctn, all child-n will be deleted too.
 */
#define kjson_push_container_array(ctn, key, ... ) do {                                                             \
    struct kjson_container *arr[] = { __VA_ARGS__ };                                                                \
    kjson_push_object(ctn, key, KOBJECT_TYPE_OBJECT_ARRAY, arr, sizeof(arr) / sizeof(struct kjson_container*));     \
} while(0)

#define kjson_push_null(ctn, key)					\
	kjson_push_object(ctn, key, KOBJECT_TYPE_OBJECT_NULL, 0, 0);	\
	
#define kjson_push_true(ctn, key) do {						\
	int true = 1;								\
	kjson_push_object(ctn, key, KOBJECT_TYPE_OBJECT_BOOL, &true, 0);	\
} while(0)

#define kjson_push_false(ctn, key) do {						\
	int false = 0;								\
	kjson_push_object(ctn, key, KOBJECT_TYPE_OBJECT_BOOL, &false, 0);	\
} while(0)
/* ****** utility read functions ****** */

/*
 * Converts a kjson_object_t to an integer.
 */
#define kjson_as_integer(obj) *((int64_t*)obj->data)

/*
 * Converts a kjson_object_t to a string.
 */
#define kjson_as_string(obj) (char*)obj->data

/*
 * Converts a kjson_object_t to a container.
 * kjson_container returned is a child (Do not deallocated it).
 */
#define kjson_as_container(obj) *((struct kjson_container**)obj->data)

/*
 * Converts a kjson_object_t to an array of integer.
 */
#define kjson_as_integer_array(obj) (int64_t*)((struct kjson_array_struct*)obj->data)->data

/*
 * Converts a kjson_object_t to an array of string.
 */
#define kjson_as_string_array(obj) (char**)((struct kjson_array_struct*)obj->data)->data

/*
 * Converts a kjson_object_t to an array of containers.
 * kjson_containers returned are childs (Do not deallocated it).
 */
#define kjson_as_container_array(obj) (struct kjson_container**)((struct kjson_array_struct*)obj->data)->data

/*
 * Get the lenght of an array.
 */
#define kjson_array_length(obj) (size_t)((struct kjson_array_struct*)obj->data)->len

/*
 * Get bool data as integer
 */
#define kjson_as_bool(obj) *((int*)obj->data)

/*
 * Return true if the JSON object is null
 */
#define kjson_object_is_null(obj) (obj->type == KOBJECT_TYPE_OBJECT_NULL)

/*
 * Create a JSON text starting from the internal data structure.
 * @ctn: A non NULL kjson_container_t
 * It returns a kjstring_t containing the KJSON Text parsed.
 * An error code is returned in case of error. (ENOMEM)
 * The kjstring_t object returned must to be released with kjstring_free function.
 */
extern struct kjstring_t *kjson_dump(struct kjson_container *ctn);

/*
 * Parse a JSON text and create the kjson_container object.
 * @json_str: The string buffer containing the json text to parse.
 * It returns a kjson_container built starting by the JSON Text contained into the json_str.
 * In case of error, NULL is returned.
 */
extern struct kjson_container *kjson_parse(const char *json_str);


#endif
