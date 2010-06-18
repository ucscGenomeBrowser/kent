include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = rikenBestInCluster.o

rikenBestInCluster: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/rikenBestInCluster $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/rikenBestInCluster${EXE}

clean::
	rm -f ${O}
