kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = axtBestNear.o

axtBestNear: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtBestNear $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/axtBestNear${EXE}
test: chr22 
%:	input/%.axt
	axtBestNear input/$@.axt $@ output/$@.axt 
	diff expected/$@.axt output/$@.axt 
	rm output/$@.axt

clean::
	rm -f ${O}
