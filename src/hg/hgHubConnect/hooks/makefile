kentSrc = ../../..
ifeq (${PROG},)
PROGS = pre-create pre-finish
sharedObjects = hooklib.o
include ${kentSrc}/inc/userAppMulti.mk
else
extraObjects = hooklib.o
extraHeaders = hooklib.h
CGI_LOADER_SUBDIR = hooks

A = ${PROG}
include ${kentSrc}/inc/cgiLoader.mk
endif
