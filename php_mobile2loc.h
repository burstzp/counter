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

#ifndef PHP_MOBILE2LOC_H
#define PHP_MOBILE2LOC_H

extern zend_module_entry mobile2loc_module_entry;
#define phpext_mobile2loc_ptr &mobile2loc_module_entry

#define PHP_MOBILE2LOC_VERSION "0.1.0" /* Replace with version number for your extension */

#ifdef PHP_WIN32
#	define PHP_MOBILE2LOC_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_MOBILE2LOC_API __attribute__ ((visibility("default")))
#else
#	define PHP_MOBILE2LOC_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(mobile2loc);
PHP_MSHUTDOWN_FUNCTION(mobile2loc);
PHP_RINIT_FUNCTION(mobile2loc);
PHP_RSHUTDOWN_FUNCTION(mobile2loc);
PHP_MINFO_FUNCTION(mobile2loc);

PHP_FUNCTION(mobile2loc);
typedef struct mobile_loc{
    char *index;
    char *index_block;
    uint index_block_len;
    ulong index_len;
    uint threshold;
    php_stream *stream;
} mobile_loc_item;
/* 
   Declare any global variables you may need between the BEGIN
   and END macros here:     
 */
ZEND_BEGIN_MODULE_GLOBALS(mobile2loc)
    char *filename;
ZEND_END_MODULE_GLOBALS(mobile2loc)
/* 
  	Declare any global variables you may need between the BEGIN
	and END macros here:     

ZEND_BEGIN_MODULE_GLOBALS(mobile2loc)
	long  global_value;
	char *global_string;
ZEND_END_MODULE_GLOBALS(mobile2loc)
*/

/* In every utility function you add that needs to use variables 
   in php_mobile2loc_globals, call TSRMLS_FETCH(); after declaring other 
   variables used by that function, or better yet, pass in TSRMLS_CC
   after the last function argument and declare your utility function
   with TSRMLS_DC after the last declared argument.  Always refer to
   the globals in your function as MOBILE2LOC_G(variable).  You are 
   encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/

#ifdef ZTS
#define MOBILE2LOC_G(v) TSRMG(mobile2loc_globals_id, zend_mobile2loc_globals *, v)
#else
#define MOBILE2LOC_G(v) (mobile2loc_globals.v)
#endif

#endif	/* PHP_MOBILE2LOC_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
