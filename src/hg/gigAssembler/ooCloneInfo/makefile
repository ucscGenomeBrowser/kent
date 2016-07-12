kentSrc = ../../..
include ../../../inc/common.mk


L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooCloneInfo.o

ooCloneInfo: $O 
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/ooCloneInfo $O $(MYLIBS) $L

clean::
	rm -f ${O}
