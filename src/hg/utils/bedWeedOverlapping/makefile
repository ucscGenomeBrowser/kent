include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm -lz
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = bedWeedOverlapping.o

bedWeedOverlapping: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/bedWeedOverlapping $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/bedWeedOverlapping${EXE}

test:
	bedWeedOverlapping inWeed.bed input.bed output.bed
	diff expected.bed output.bed
	
clean:
	rm -f $O
