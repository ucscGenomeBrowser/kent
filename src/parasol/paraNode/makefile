include ../../inc/common.mk

O = paraNode.o

paraNode: $(O)
	${MKDIR} ../bin
	${CC} ${COPT} ${CFLAGS} -o ../bin/paraNode $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a
	${STRIP} ../bin/paraNode${EXE}

clean::
	rm -f ${O}
