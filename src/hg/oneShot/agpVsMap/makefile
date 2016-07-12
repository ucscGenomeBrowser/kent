kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = agpVsMap.o

agpVsMap: $O 
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/agpVsMap $O $(MYLIBS) $L

clean::
	rm -f ${O}
