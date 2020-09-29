#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([sibyl], [0.8.3.2], [den@dendrown.net])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([m4])

AM_INIT_AUTOMAKE
AM_SILENT_RULES([yes])
AC_CONFIG_FILES([Makefile
                 etc/Makefile
                 src/Makefile
                 src/genprog/Makefile
                 src/market/Makefile
                 src/util/Makefile])

# OpenMPI Support
AC_ARG_ENABLE([mpi],
              [AS_HELP_STRING([--disable-mpi],
                              [Do not use cluster/MPI architecture. The exectuable will run on a single compute node.])],
              [mpi=${enableval}],
              [mpi=yes])

AS_IF([test "x$mpi" = "xyes"],
[
    AC_CHECK_PROG([MPICPP], [mpic++], [mpic++])
    AS_IF([test "x${MPICPP}" == "xmpic++"],
    [
        AC_SUBST(CXX, mpic++)
        AC_SUBST(CC,  mpicc)
        AC_DEFINE([ENABLE_MPI], 1, [Use cluster/MPI architecture])
    ],
    [
        AC_MSG_ERROR([
            -------------------------------------------------------------------
            mpic++ not found! Please rerun configure, specifying: --disable-mpi
            -------------------------------------------------------------------])
    ])
])

# Checks for programs.
AC_PROG_CXX
AC_SUBST([AM_CPPFLAGS], ["-std=gnu++14 -Wall"])

# Cuda Support
AC_ARG_WITH([cuda],
            [AS_HELP_STRING([--with-cuda],
                            [Use Cuda from the specified directory.])],
            [with_cuda=${withval}],
            [with_cuda=no])
AC_ARG_ENABLE([cuda],
              [AS_HELP_STRING([--enable-cuda],
                              [Use NVidia/cuda GPU coprocessor])],
              [enable_cuda=${enableval}],
              [enable_cuda=no])
AC_ARG_WITH([cuda-arch],
            [AS_HELP_STRING([--with-cuda-arch],
                            [Use the specified cuda architecture (minimum sm_50)])],
            [with_cuda_arch=${withval}],
            [with_cuda_arch=sm_50])

AS_IF([test "x$enable_cuda" = "xyes" -a "x$with_cuda" = "xno"], [with_cuda=yes])

AS_IF([test "x$with_cuda" != "xno"],
[
    AS_IF([test "x$with_cuda" = "xyes" || "x$with_cuda" = "x"], [with_cuda=/opt/cuda])

    AC_SUBST([NVCC],                [$with_cuda/bin/nvcc])
    AC_SUBST([CUDA_DLINK_CFLAGS],   ["-arch=$with_cuda_arch"])
    AC_SUBST([CUDA_CFLAGS],         ["-arch=$with_cuda_arch -I$with_cuda/include"])
    AC_SUBST([CUDA_LIBS],           ["-L$with_cuda/lib64 -lcudart"])
    dnl AC_SUBST([CUFFT_LIBS],      ["-L$with_cuda/lib64 -lcufft"])
    dnl AC_SUBST([CUBLAS_LIBS],     ["-L$with_cuda/lib64 -lcublas"])

    # Compilation flags for non-cuda (gcc/g++) code, which interfaces with Cuda code
    AC_SUBST([CUDA_GCC_CFLAGS],   ["-I$with_cuda/include"])

    # NVCC doesn't support C++11
    #AC_SUBST([CUDA_AM_CPPFLAGS], [${AM_CPPFLAGS#-std=gnu++11}])

    # Do a test compile
    AC_MSG_CHECKING([for nvcc compilation])
    cuda_ok=no

    cat>conftest.cu<<EOF

    __global__ static void test_cuda()
    {
        const int tid = threadIdx.x;
        const int bid = blockIdx.x;
        __syncthreads();
    }
EOF
    if $NVCC -c $CUDA_CFLAGS conftest.cu &> /dev/null; then
        cuda_ok=yes
    fi
    AC_MSG_RESULT([$cuda_ok])

    AS_IF([test "x$cuda_ok" = "xno"],
    [
        AC_MSG_ERROR([
            ----------------------------------------------
            Cuda compilation failed for architecture $with_cuda_arch             
            ----------------------------------------------])
    ])
    AC_DEFINE([ENABLE_CUDA], 1, [Use NVida CUDA GPU coprocessor])
])
AM_CONDITIONAL([WANT_CUDA], [test  x$cuda_ok = xyes])

# Project options
AC_ARG_ENABLE([adj-close],
              [AS_HELP_STRING([--enable-adj-close],
                              [Allow Sibyl to make use of adjusted close data from Delphi])])

AS_IF([test "x$enable_adj_close" = "xyes"],
      [AC_DEFINE([ENABLE_ADJ_CLOSE], 1, [Use adjusted close data from Delphi])])


AC_ARG_ENABLE([pe-ratio],
              [AS_HELP_STRING([--enable-pe-ratio],
                              [Allow Sibyl to make use of Price/Earnings data from Delphi])])

AS_IF([test "x$enable_pe_ratio" = "xyes"],
      [AC_DEFINE([ENABLE_PE_RATIO], 1, [Use Price/Earnings data from Delphi])])



# Non-optional project decisions
AC_DEFINE([OI_XTN_GENPROG],[],[Use genprog extensions (for market)])


# Checks for libraries.
#
# Using libtool (sigh)
LT_INIT
PKG_CHECK_MODULES([libpqxx], [libpqxx])

# Boost.m4: @ref https://github.com/tsuna/boost.m4
BOOST_REQUIRE([1.49.0])
BOOST_FOREACH                           # Boost header-only...
BOOST_DATE_TIME                         # Boost library...
BOOST_SERIALIZATION
BOOST_PROGRAM_OPTIONS
BOOST_THREAD
AS_IF([test "x$mpi" = "xyes"],
      [BOOST_MPI([openmpi])])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_CHECK_FUNCS([gethostname localtime_r memset pow strdup])

# User feedback
AC_MSG_NOTICE([--------------------------------------------------------------------])
AC_MSG_NOTICE([General build settings:])
AC_MSG_NOTICE([  CPPFLAGS         : $CPPFLAGS])
AC_MSG_NOTICE([  CFLAGS           : $CFLAGS])
AC_MSG_NOTICE([  CXXFLAGS         : $CXXFLAGS])

AC_MSG_NOTICE([])
AS_IF([test "x$mpi" = "xyes"],
[
    AC_MSG_NOTICE([MPI settings:])
    AC_MSG_NOTICE([  CXX              : $(which $CXX)])
    AC_MSG_NOTICE([  CC               : $(which $CC)])
],[ AC_MSG_NOTICE([MPI support disabled]) ])

AC_MSG_NOTICE([])
AS_IF([test "x$with_cuda" != "xno"],
[
    AC_MSG_NOTICE([Cuda settings:])
    AC_MSG_NOTICE([  NVCC             : $NVCC])
    ##_MSG_NOTICE([  CUDA_AM_CPPFLAGS : $CUDA_AM_CPPFLAGS])
    AC_MSG_NOTICE([  CUDA_CFLAGS      : $CUDA_CFLAGS])
    AC_MSG_NOTICE([  CUDA_LIBS        : $CUDA_LIBS])
    ##_MSG_NOTICE([  CUFFT_LIBS       : $CUFFT_LIBS])
    ##_MSG_NOTICE([  CUBLAS_LIBS      : $CUBLAS_LIBS])
],[ AC_MSG_NOTICE([Cuda support not requested])])
AC_MSG_NOTICE([--------------------------------------------------------------------])

# Make it so...!
AC_OUTPUT
