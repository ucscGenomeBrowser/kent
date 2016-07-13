kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ggcPic.o

ggcPic: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/ggcPic $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/ggcPic${EXE}

clean::
	rm -f ${O}
