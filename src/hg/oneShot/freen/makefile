include ../../../inc/common.mk

HG_WARN = ${HG_WARN_ERR}

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = freen.o

hello: freen.o
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/freen $O $(MYLIBS) $L

