include ../../inc/common.mk

MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS = $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
LIBS= ${MYLIBS} -lm

A = pslMap
O = pslMap.o

all: ${BINDIR}/pslMap


pslMap: ${O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

pslMap.o: usage.msg

# create usage msg from text
usage.msg: usage.txt
	echo '/* GENERATED CODE, DO NOT EDIT */' > $@.tmp
	${STRINGIFY} $< >> $@.tmp
	mv -f $@.tmp $@

compile:: ${O} usage.msg
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} $O ${MYLIBS} ${L}

test::
	(cd tests && ${MAKE} test)

clean::
	rm -f pslMap *.o usage.msg
	(cd tests && ${MAKE} clean)
