include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = axtDropOverlap.o 

axtDropOverlap: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtDropOverlap $O ${MYLIBS} $L
#${STRIP} ${BINDIR}/axtDropOverlap${EXE}

lib:
	cd ../../lib && ${MAKE}

test:	test1 test2
%:	input/%.axt
	${MKDIR} output
	axtDropOverlap input/$@.axt input/H input/H output/$@.axt
	diff output/$@.axt expected/$@.axt
	rm output/$@.axt

clean::
	rm -fr output ${O}
