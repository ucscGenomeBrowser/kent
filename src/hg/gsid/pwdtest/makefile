kentSrc = ../../..
include ../../../inc/common.mk
# !!! NOTE we use gcc34 here to avoid a compiler compatibility issue with openss
CC = gcc34

XINC = -I/usr/include
L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS} -lcrypt -lssl
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
A = pwdtest

O = $A.o 

HG_WARN=${HG_WARN_ERR}

my: compile
	chmod a+rx $A${EXE}
	mv $A${EXE} ${CGI_BIN}-signup/$A

compile: $O $(MYLIBS)
	${CC} ${COPT} -o $A $O $(MYLIBS) $L

clean::
	rm -f ${O}
