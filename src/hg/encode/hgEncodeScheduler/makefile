kentSrc = ../../..
include ../../../inc/common.mk


L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS} -lcrypt
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgEncodeScheduler.o

hgEncodeScheduler: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgEncodeScheduler $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgEncodeScheduler${EXE}

test:
	hgEncodeScheduler encpipeline_dev /cluster/store3/encpipeline_dev

clean:
	rm $O
