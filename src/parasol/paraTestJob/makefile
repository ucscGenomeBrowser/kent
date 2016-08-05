kentSrc = ../..
include ../../inc/common.mk

L += -lm -lz
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = paraTestJob.o

paraTestJob: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ../bin/paraTestJob $O $(MYLIBS) $L
	${STRIP} ../bin/paraTestJob${EXE}

clean::
	rm -f ${O}
