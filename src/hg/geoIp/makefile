kentSrc = ../..
include ${kentSrc}/inc/common.mk

HG_INC+=-I${kentSrc}/inc

L += -lm ${SOCKETLIB}
MYLIBDIR = ${kentSrc}/lib/${MACHTYPE}
MYLIBS = $(MYLIBDIR)/jkweb.a 

A = geoIpToCountryMaxMind

O = $(A).o

$(A): ${O} ${MYLIBS}
	@${MKDIR} "${DESTDIR}${BINDIR}"
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

clean::
	rm -f ${O}

