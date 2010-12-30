include ../../../inc/common.mk

MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS = $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
L += -lm

A = pslMap
O = pslMap.o

all: ${BINDIR}/pslMap


${BINDIR}/pslMap: ${O} ${MYLIBS}
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

test:: compile
	(cd tests && ${MAKE} test)
	rm -f ${A}${EXE}

clean::
	rm -f ${A} ${O} usage.msg
	(cd tests && ${MAKE} clean)
