kentSrc = ../../..
include ../../../inc/common.mk


L +=  $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mendMap.o

mendMap: $O 
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/mendMap $O $(MYLIBS) $L

clean::
	rm -f ${O}
