include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS = $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
LIBS= ${MYLIBS} -lm

BINDIR= $(HOME)/bin/$(MACHTYPE)
OBJS = pslMap.o

all: ${BINDIR}/pslMap

${BINDIR}/pslMap: pslMap
	${MKDIR} ${BINDIR}
	cp -f pslMap ${BINDIR}
	${STRIP} ${BINDIR}/pslMap${EXE}

pslMap: ${OBJS} ${MYLIBS}
	${CC} ${CFLAGS} ${COPT} -o pslMap ${OBJS} ${LIBS} 

test::
	(cd tests && ${MAKE} test)

clean::
	rm -f pslMap *.o
	(cd tests && ${MAKE} clean)
