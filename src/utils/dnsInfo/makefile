kentSrc = ../..
include ../../inc/common.mk

L += -lm -lresolv
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = dnsInfo.o

dnsInfo: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/dnsInfo $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/dnsInfo${EXE}

clean::
	rm -f ${O}
