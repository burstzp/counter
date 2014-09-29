/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2014 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_mobile2loc.h"

#define INDEX_LEN_READ_NUM 4
#define PHONE_NUM          11 
#define THRESHOLD          1200
ZEND_DECLARE_MODULE_GLOBALS(mobile2loc)

/* True global resources - no need for thread safety here */
static int le_mobile2loc_persistent;
#define MOBILE_HASH_KEY_NAME "mobile2loc_area"
#define MOBILE_DATA_RES_NAME "mobile2loc_area_dtor"
char machine_little_endian;

#define mobile_part2int_convert(phone, mobile, length) do {\
    char *tmp = NULL;\
    tmp = substr(mobile, 0, length);\
    phone = atoi(tmp);  \
    efree(tmp);\
    tmp = NULL;\
} while(0)

/* {{{ mobile2loc_functions[]
 *
 * Every user visible function must have an entry in mobile2loc_functions[].
 */
const zend_function_entry mobile2loc_functions[] = {
	PHP_FE(mobile2loc, NULL)
    PHP_FE_END	/* Must be the last line in mobile2loc_functions[] */
};
/* }}} */

/* {{{ mobile2loc_module_entry
 */
zend_module_entry mobile2loc_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"mobile2loc",
	mobile2loc_functions,
	PHP_MINIT(mobile2loc),
	PHP_MSHUTDOWN(mobile2loc),
	PHP_RINIT(mobile2loc),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(mobile2loc),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(mobile2loc),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_MOBILE2LOC_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_MOBILE2LOC
ZEND_GET_MODULE(mobile2loc)
#endif

PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("mobile2loc.filename", "", PHP_INI_ALL, OnUpdateString, filename, zend_mobile2loc_globals, mobile2loc_globals)
PHP_INI_END()

/*{{{*/
static int lb_reverse(int a){
    union {
        int i;
        char c[4];
    }u, r;
    u.i = a;
    r.c[0] = u.c[3];
    r.c[1] = u.c[2];
    r.c[2] = u.c[1];
    r.c[3] = u.c[0];
    return r.i;
}
/*}}}*/
/*{{{*/
static char *substr(const char *str, int position, int length) {
    char *pointer;
    pointer = emalloc(length+1);

    int i;
    for (i=0; i < position; i++) {
        str++;
    }
    for (i=0; i < length; i++) {
        *(pointer+i) = *str;
        str++;
    }
    *(pointer+i) = '\0';
    return pointer;
}
/*}}}*/
/*{{{*/
mobile_loc_item *mobile2loc_init(INTERNAL_FUNCTION_PARAMETERS) {
    php_stream *stream; 
    stream = php_stream_open_wrapper(MOBILE2LOC_G(filename), "rb", ENFORCE_SAFE_MODE | STREAM_OPEN_PERSISTENT | REPORT_ERRORS, NULL);
    if (!stream) {
        return NULL;
    }

    mobile_loc_item *loc = NULL;
    loc = (mobile_loc_item *)pemalloc(sizeof(mobile_loc_item), 1);
    if (!loc) {
        php_stream_pclose(stream);
        return NULL;
    }

    int offset = 0;
    php_stream_rewind(stream);
    php_stream_read(stream, (char *)&offset, INDEX_LEN_READ_NUM);
    if (!machine_little_endian) {
        offset = lb_reverse(offset);
    }

    uint index_block_len;
    php_stream_read(stream, (char *)&index_block_len, INDEX_LEN_READ_NUM);
    if (!index_block_len) {
        pefree(loc, 1);
        php_stream_pclose(stream);
        return NULL;
    }
    
    loc->index_block = (char *)pemalloc(sizeof(char) * index_block_len + 1, 1);
    if (!loc->index_block) {
        pefree(loc, 1);
        php_stream_pclose(stream);
        return NULL;
    }
    
    loc->stream             = stream;
    
    if (!php_stream_read(stream, loc->index_block, index_block_len)) {
        goto INIT_ERR;
    }
    
    loc->index_block_len    = index_block_len;
    loc->threshold          = THRESHOLD;
    loc->index_len          = offset - INDEX_LEN_READ_NUM * 2 - index_block_len;
    loc->index              = (char *)pemalloc(sizeof(char) * loc->index_len + 1, 1);
    
    if (!loc->index) {
        goto INIT_ERR;
    }
    
    if (!php_stream_read(stream, loc->index, loc->index_len)) {
        goto RES_ERR;
    }
    
    list_entry le;
    le.type = le_mobile2loc_persistent;
    le.ptr = loc;
    if (zend_hash_update(&EG(persistent_list), MOBILE_HASH_KEY_NAME, sizeof(MOBILE_HASH_KEY_NAME), (void *)&le, sizeof(list_entry), NULL) == FAILURE) {
        goto RES_ERR;
    }
    
    return loc;

INIT_ERR:
    php_stream_pclose(loc->stream);
    pefree(loc->index_block, 1);
    pefree(loc, 1);
    return NULL;

RES_ERR:
    php_stream_pclose(loc->stream);
    pefree(loc->index_block, 1);
    pefree(loc->index, 1);
    pefree(loc, 1);
    return NULL;
}
/*}}}*/
/*{{{*/
static void php_mobile2loc_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) {
    mobile_loc_item *rs = (mobile_loc_item *)rsrc->ptr;
    pefree(rs->index_block, 1);
    pefree(rs->index, 1);
    php_stream_pclose(rs->stream);
    pefree(rs, 1);
}
/*}}}*/
/* {{{ 
 */
static int first_index_search(const mobile_loc_item *loc, int search) {
    int first_index_key, start;
    uint second_index_offset = 0;
    for (start = 0; start < loc->index_block_len; start += 8) {
        memcpy(&first_index_key, loc->index_block + start, 4);
        if (!machine_little_endian) {
            first_index_key = lb_reverse(first_index_key);
        }

        if (first_index_key == search) {
            memcpy(&second_index_offset, loc->index_block + start + 4, 4);
            if (!machine_little_endian) {
                second_index_offset = lb_reverse(second_index_offset);
            }
            return second_index_offset;
        }
    }
    return -1;
}
/* }}} */

/*{{{*/
PHP_FUNCTION(mobile2loc) {
    int argc = ZEND_NUM_ARGS();
    char *phone_number;
    int phone_number_len;
    if (zend_parse_parameters(argc TSRMLS_CC, "s", &phone_number, &phone_number_len) == FAILURE) {
        RETURN_FALSE;
    }

    if (phone_number_len != PHONE_NUM) {
        RETURN_FALSE;
    }

    list_entry *le;
    mobile_loc_item *loc = NULL;
    if (zend_hash_find(&EG(persistent_list), ZEND_STRS(MOBILE_HASH_KEY_NAME), (void **)&le) == SUCCESS){
        loc = (mobile_loc_item *)le->ptr;
    } else {
        loc = mobile2loc_init(INTERNAL_FUNCTION_PARAM_PASSTHRU);
    }
    
    if (!loc) {
        RETURN_FALSE;
    }
    
    int second_index_key, first_index_key;
    mobile_part2int_convert(second_index_key, phone_number, 7);
    mobile_part2int_convert(first_index_key, phone_number, 4);

    int start = first_index_search(loc, first_index_key);
    if (start == -1) {
        RETURN_FALSE;
    }
   
    uint index_offset = 0;
    signed char index_length;
    
    char *phone;
    phone = (char *)&second_index_key;
    int loop_num = 0;
    for (; start < loc->index_len; start += 9) {
        if (memcmp(loc->index + start, phone, 4) == 0) {
            memcpy(&index_offset, loc->index + start + 4, 4);
            if (!machine_little_endian) {
                index_offset = lb_reverse(index_offset);
            }
            index_length = *(loc->index + start + 8);
            break;
        }

        if (loop_num++ > loc->threshold) {
            RETURN_FALSE;
        }
    }
    if (!loc->stream || php_stream_seek(loc->stream, loc->index_len + 8 + loc->index_block_len + index_offset, SEEK_SET) == FAILURE) {
        RETURN_FALSE;
    }

    Z_STRVAL_P(return_value) = emalloc(index_length + 1);
    if (!php_stream_read(loc->stream, Z_STRVAL_P(return_value), index_length)) {
        efree(Z_STRVAL_P(return_value));
        RETURN_FALSE;
    } 
    Z_STRLEN_P(return_value) = index_length;
    Z_STRVAL_P(return_value)[Z_STRLEN_P(return_value)] = 0;
    Z_TYPE_P(return_value) = IS_STRING;
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(mobile2loc)
{
	REGISTER_INI_ENTRIES();
    int machine_endian_check = 1;
    machine_little_endian = ((char *)&machine_endian_check)[0];
    le_mobile2loc_persistent = zend_register_list_destructors_ex(NULL, php_mobile2loc_dtor, MOBILE_DATA_RES_NAME, module_number); 
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(mobile2loc)
{
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(mobile2loc)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(mobile2loc)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(mobile2loc)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "mobile2loc support", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
