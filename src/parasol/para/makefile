kentSrc = ../..
include ../../inc/common.mk

L += -lm -lz
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = para.o jobDb.o

../bin/para: $O $(MYLIBS)
	@${MKDIR} ../bin
	${CC} ${COPT} ${CFLAGS} -o ../bin/para $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a ${L}
	${STRIP} ../bin/para${EXE}

clean::
	rm -f ../bin/para $O
