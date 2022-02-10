# Generated by devtools/yamaker. 
 
LIBRARY() 
 
OWNER(g:cpp-contrib) 
 
LICENSE(Apache-2.0) 
 
LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR( 
    contrib/restricted/abseil-cpp/absl/base 
    contrib/restricted/abseil-cpp/absl/base/internal/low_level_alloc 
    contrib/restricted/abseil-cpp/absl/base/internal/raw_logging 
    contrib/restricted/abseil-cpp/absl/base/internal/spinlock_wait 
    contrib/restricted/abseil-cpp/absl/base/log_severity 
) 
 
ADDINCL( 
    GLOBAL contrib/restricted/abseil-cpp 
) 
 
NO_COMPILER_WARNINGS() 
 
NO_UTIL() 
 
CFLAGS(
    -DNOMINMAX
)

SRCS( 
    graphcycles.cc 
) 
 
END() 
