include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L = -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = paraNodeStart.o

paraNodeStart: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ../bin/paraNodeStart $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a
	${STRIP} ../bin/paraNodeStart${EXE}
