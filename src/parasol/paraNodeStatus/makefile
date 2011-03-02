include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = paraNodeStatus.o

paraNodeStatus: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ../bin/paraNodeStatus $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a ${L}
	${STRIP} ../bin/paraNodeStatus${EXE}

clean::
	rm -f ${O}
