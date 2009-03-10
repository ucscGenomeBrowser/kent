include ../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = bedOrBlocks.o

bedOrBlocks: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bedOrBlocks $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bedOrBlocks${EXE}

test:
	bedOrBlocks in.bed out.bed
	diff expected.bed out.bed

clean:
	rm -f $O
