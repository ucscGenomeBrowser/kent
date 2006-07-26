include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
BIN=$(HOME)/bin/$(MACHTYPE)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = vegaBuildInfo.o

all: ${BIN}/vegaBuildInfo 

${BIN}/vegaBuildInfo: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BIN}/vegaBuildInfo $O $(MYLIBS) $L

