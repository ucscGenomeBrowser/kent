include ../../inc/common.mk

L += -lm ${SOCKETLIB}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = $(MYLIBDIR)/jkweb.a 

A = paraSync

O = $(A).o

$(A): ${O} ${MYLIBS}
	@${MKDIR} "${DESTDIR}${BINDIR}"
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

clean::
	rm -f ${O}

install:
	${MAKE} BINDIR=/cluster/bin/${MACHTYPE}

