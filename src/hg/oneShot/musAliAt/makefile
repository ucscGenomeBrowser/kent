kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS} 
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = musAliAt.o

musAliAt: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o musAliAt $O $(MYLIBS) $L

clean::
	rm -f ${O}
