# Generated by devtools/yamaker. 
 
LIBRARY() 
 
WITHOUT_LICENSE_TEXTS()
 
OWNER(
    somov
    g:cpp-contrib
)

LICENSE(Apache-2.0) 
 
ADDINCL(
    GLOBAL contrib/restricted/abseil-cpp-tstring
)
 
NO_COMPILER_WARNINGS() 
 
SRCDIR(contrib/restricted/abseil-cpp-tstring/y_absl/profiling/internal)
 
SRCS( 
    exponential_biased.cc 
) 
 
END() 
