include ../../../inc/common.mk

MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS = $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
L += -lm

A = pslMap
O = pslMap.o

all: ${BINDIR}/pslMap


${BINDIR}/${A}${EXE}: ${O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}
	@rm -f usage.msg

pslMap.o: usage.msg

# create usage msg from text
usage.msg: usage.txt
	echo '/* GENERATED CODE, DO NOT EDIT */' > $@.tmp
	${STRINGIFY} $< >> $@.tmp
	mv -f $@.tmp $@

compile:: ${BINDIR}/${A}${EXE}

test:: compile
	(cd tests && ${MAKE} test)

clean::
	rm -f ${A} ${O} usage.msg
	(cd tests && ${MAKE} clean)
