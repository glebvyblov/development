# Generated by devtools/yamaker.

LIBRARY()

WITHOUT_LICENSE_TEXTS()

OWNER(
    dldmitry
    efmv
    kikht
    g:cpp-contrib
)

LICENSE(BSD-3-Clause) 
 
PEERDIR(
    contrib/libs/libc_compat
)

ADDINCL(
    contrib/libs/libevent
    contrib/libs/libevent/include
)

NO_COMPILER_WARNINGS()

NO_RUNTIME()

CFLAGS(
    -DHAVE_CONFIG_H
    -DEVENT__HAVE_STRLCPY=1
)

SRCDIR(contrib/libs/libevent)

SRCS(
    buffer.c
    bufferevent.c
    bufferevent_filter.c
    bufferevent_pair.c
    bufferevent_ratelim.c
    bufferevent_sock.c
    event.c
    evmap.c
    evthread.c
    evutil.c
    evutil_rand.c
    evutil_time.c
    listener.c
    log.c
    signal.c
)

IF (OS_WINDOWS)
    SRCS(
        buffer_iocp.c
        bufferevent_async.c
        event_iocp.c
        win32select.c
    )
ELSE()
    SRCS(
        poll.c
        select.c
    )
ENDIF()

IF (OS_LINUX)
    SRCS(
        epoll.c
    )
ENDIF()

IF (OS_FREEBSD OR OS_DARWIN)
    SRCS(
        kqueue.c
    )
ENDIF()

END()
