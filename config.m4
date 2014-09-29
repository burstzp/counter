dnl $Id$
dnl config.m4 for extension mobile2loc

PHP_ARG_ENABLE(mobile2loc, whether to enable mobile2loc support,
[  --enable-mobile2loc           Enable mobile2loc support])

if test "$PHP_MOBILE2LOC" != "no"; then
  PHP_NEW_EXTENSION(mobile2loc, mobile2loc.c, $ext_shared)
fi
