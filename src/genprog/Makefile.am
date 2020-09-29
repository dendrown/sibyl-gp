AM_CPPFLAGS = @AM_CPPFLAGS@ ${CUDA_GCC_CFLAGS}

noinst_LTLIBRARIES = libgenprog.la

libgenprog_la_CPPFLAGS  = -DSHAREDSTATEDIR='"$(sharedstatedir)"' \
                          $(AM_CPPFLAGS)                         \
                          $(BOOST_CPPFLAGS)
libgenprog_la_LDFLAGS   = $(BOOST_PROGRAM_OPTIONS_LDFLAGS) $(BOOST_THREAD_LDFLAGS)
libgenprog_la_LIBS      = $(BOOST_PROGRAM_OPTIONS_LIBS)    $(BOOST_THREAD_LIBS)

libgenprog_la_SOURCES = Allele.cpp              \
                        Attribute.cpp           \
                        AttrWindow.cpp          \
                        ConstAllele.cpp         \
                        FuncAllele.cpp          \
                        EliteTournament.cpp     \
                        GPFunction.cpp          \
                        Individual.cpp          \
                        LookupAllele.cpp        \
                        RouletteTournament.cpp  \
                        Splice.cpp              \
                        World.cpp               \
                        WorldVR.cpp             \
                        test.cpp
