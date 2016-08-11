kentSrc = ../../..
include ../../../inc/common.mk


L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooCloneSizes.o

ooCloneSizes: $O 
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/ooCloneSizes $O $(MYLIBS) $L

clean::
	rm -f ${O}
