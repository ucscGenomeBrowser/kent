kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm -lz
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = repeatSelect
O = ${A}.o bitHash.o repeatGroup.o rmskOutExtra.o

compile: $(O) $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${A} $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/${A}${EXE}

clean:
	rm -f $(O)
