# libfuse: disabled by default
ifeq (${USE_FUSE},1)
    L+=-lfuse
    HG_DEFS+=-DUSE_FUSE
endif

