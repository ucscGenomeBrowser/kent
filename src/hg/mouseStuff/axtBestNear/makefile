include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = axtBestNear.o

axtBestNear: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/axtBestNear $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/axtBestNear${EXE}
test: chr22 
%:	input/%.axt
	axtBestNear input/$@.axt $@ output/$@.axt 
	diff expected/$@.axt output/$@.axt 
	rm output/$@.axt
