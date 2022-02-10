LIBRARY()

LICENSE(MIT) 

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

OWNER(
    pg
    g:contrib
    g:cpp-contrib
)

NO_UTIL()

NO_COMPILER_WARNINGS()

ADDINCL(contrib/libs/brotli/include)

SRCS(
    dictionary.c
    transform.c
)

CFLAGS(-DBROTLI_BUILD_PORTABLE)

END()
