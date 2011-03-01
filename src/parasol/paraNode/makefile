include ../../inc/common.mk

O = paraNode.o

paraNode: $(O)
	${MKDIR} ../bin
	${CC} ${COPT} ${CFLAGS} -o ../bin/paraNode $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a ${L}
	${STRIP} ../bin/paraNode${EXE}

clean::
	rm -f ${O}
