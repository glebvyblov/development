# Generated by devtools/yamaker. 
 
LIBRARY() 
 
WITHOUT_LICENSE_TEXTS()
 
OWNER(
    somov
    g:cpp-contrib
)

LICENSE(Apache-2.0) 
 
PEERDIR( 
    contrib/restricted/abseil-cpp-tstring/y_absl/base
    contrib/restricted/abseil-cpp-tstring/y_absl/base/internal/raw_logging
    contrib/restricted/abseil-cpp-tstring/y_absl/base/internal/spinlock_wait
    contrib/restricted/abseil-cpp-tstring/y_absl/base/log_severity
) 
 
ADDINCL(
    GLOBAL contrib/restricted/abseil-cpp-tstring
)
 
NO_COMPILER_WARNINGS() 
 
SRCDIR(contrib/restricted/abseil-cpp-tstring/y_absl/debugging/internal)
 
SRCS( 
    demangle.cc 
) 
 
END() 
