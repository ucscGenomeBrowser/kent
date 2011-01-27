include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

H = rowReader.h chromAnn.h chromAnnMap.h selectTable.h coordCols.h
O = rowReader.o chromAnn.o chromAnnMap.o selectTable.o coordCols.o  overlapSelect.o
A =  overlapSelect

$A: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}
	@rm -f usage.msg

chromAnn.o: rowReader.c rowReader.h
chromAnn.o: chromAnn.c chromAnn.h rowReader.h
coordCols.o: coordCols.c coordCols.h
selectTable.o: selectTable.c selectTable.h coordCols.h chromAnn.h rowReader.h
overlapSelect.o: usage.msg overlapSelect.c coordCols.h selectTable.h chromAnn.h

# create usage msg from text
usage.msg: usage.txt
	echo '/* GENERATED CODE, DO NOT EDIT */' > $@.tmp
	${STRINGIFY} $< >> $@.tmp
	mv -f $@.tmp $@

compile: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} $O ${MYLIBS} ${L}

test: compile
	cd tests && ${MAKE} test
	rm -f ${A}${EXE} 

clean:
	rm -f $O $B usage.msg overlapSelect
	cd tests && ${MAKE} clean
