include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = chainToAxt.o

chainToAxt: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/chainToAxt $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/chainToAxt${EXE}

test: chrM
%:	input/%.chain
	${MKDIR} output
	chainToAxt input/$@.chain /gbdb/hg16/nib /gbdb/mm3/nib output/$@.axt
	diff output/$@.axt expected/$@.axt
	rm output/$@.axt

clean::
	rm -fr output ${O}
