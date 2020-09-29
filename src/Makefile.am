AM_CPPFLAGS = @AM_CPPFLAGS@ ${CUDA_GCC_CFLAGS}

bin_PROGRAMS = sibyl

sibyl_SOURCES  = main.cpp


sibyl_CPPFLAGS = -DSYSCONFDIR='"$(sysconfdir)"'   \
                 $(AM_CPPFLAGS)                   \
                 $(BOOST_CPPFLAGS)

sibyl_LDFLAGS  = $(BOOST_DATE_TIME_LDFLAGS)       \
                 $(BOOST_MPI_LDFLAGS)             \
                 $(BOOST_PROGRAM_OPTIONS_LDFLAGS) \
                 $(BOOST_SERIALIZATION_LDFLAGS)   \
                 $(BOOST_THREAD_LDFLAGS)          \
		 $(libpqxx_LDFLAGS)

sibyl_LDADD    = $(BOOST_DATE_TIME_LIBS)        \
                 $(BOOST_MPI_LIBS)     		\
                 $(BOOST_PROGRAM_OPTIONS_LIBS)  \
                 $(BOOST_SERIALIZATION_LIBS)	\
                 $(BOOST_THREAD_LIBS)  		\
		 $(libpqxx_LIBS)		\
		 $(CUDA_LIBS)		        \
                 genprog/libgenprog.la 		\
                 market/libmarket.la   		\
                 util/libutil.la

SUBDIRS = genprog market util

if WANT_CUDA
SUFFIXES = .cu

# FIXME: It would be better to specify the sources, obviously, but autotools
# 	 seems to insists on shoving them all at NVCC to be compiled at once.
# 	 Got a (seemingly) working setup for the time being, and we will come
# 	 back to improve this at a later point.
CUDA_OBJECTS = gpgpu-nv.o market/PriceDataPack-nv.o

sibyl_LDADD += ${CUDA_OBJECTS} nv-dlink.o $(CUDA_LIBS)

nv-dlink.o: ${CUDA_OBJECTS}
	$(LIBTOOL) --tag=CXX --mode=compile $(NVCC) --device-link $(CUDA_DLINK_CFLAGS) -o $@ $^ -static

.cu.o:
	$(LIBTOOL) --tag=CXX --mode=compile $(NVCC) --device-c $(DEFAULT_INCLUDES) $(CXXFLAGS) $(CPPFLAGS) $(CUDA_CFLAGS) $(DEFS) -o $@ $^ -static
endif
