include ../../../inc/common.mk


L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooNtInput.o

ooNtInput: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/ooNtInput $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/ooNtInput${EXE}


lib:
	cd ../../lib && ${MAKE}
	cd ../../../lib && ${MAKE}
