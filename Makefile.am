ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -Wall -std=gnu++11
CUDA_CFLAGS = @CUDA_CFLAGS@
NVCC = @NVCC@

SUBDIRS=src etc

do_perms =                                 \
  @if grep "^sibyl:" /etc/passwd; then     \
    chown -R sibyl:sibyl $(prefix);        \
    chmod 0664 $(sysconfdir)/*;            \
    chmod 2775 $(sharedstatedir)/log;      \
    chmod 2775 $(sharedstatedir)/prophecy; \
    chmod 2775 $(sharedstatedir)/www;      \
  fi

install-data-hook:
	$(MKDIR_P) $(sharedstatedir)/log
	$(MKDIR_P) $(sharedstatedir)/prophecy
	$(MKDIR_P) $(sharedstatedir)/www
	$(do_perms)

