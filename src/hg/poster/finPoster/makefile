kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = finPoster.o

finPoster: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o finPoster $O $(MYLIBS) $L
	${STRIP} finPoster${EXE}

poster:
	finPoster chr1 chr2 chr3 chr4 chr5 chr6 chr7 chr8 chr9 chr10 chr11 chr12 chr13 chr14 chr15 chr16 chr17 chr18 chr19 chr20 chr21 chr22 chrX chrY

test:
	finPoster chr22

clean::
	rm -f ${O}
