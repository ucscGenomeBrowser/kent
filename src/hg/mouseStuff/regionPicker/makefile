include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = regionPicker.o

regionPicker: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/regionPicker $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/regionPicker${EXE}

tags:
	ctags *.c $(HOME)/src/lib/*.c $(HOME)/src/inc/*.h

clean::
	rm -f ${O}
