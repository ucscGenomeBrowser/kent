include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a


H = rowReader.h chromAnn.h selectTable.h coordCols.h
O = rowReader.o chromAnn.o selectTable.o coordCols.o overlapSelect.o
B =  ${HOME}/bin/${MACHTYPE}/overlapSelect

$B: overlapSelect
	@${MKDIR} `dirname $B`
	cp -f overlapSelect $B

overlapSelect: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o overlapSelect $O ${MYLIBS} ${MYSQLLIBS} $L

chromAnn.o: rowReader.c rowReader.h
chromAnn.o: chromAnn.c chromAnn.h rowReader.h
coordCols.o: coordCols.c coordCols.h
selectTable.o: selectTable.c selectTable.h coordCols.h chromAnn.h rowReader.h
overlapSelect.o: usage.msg overlapSelect.c coordCols.h selectTable.h chromAnn.h

# create usage msg from text
usage.msg: usage.txt
	echo '/* GENERATED CODE, DO NOT EDIT */' > $@.tmp
	stringify $< >> $@.tmp
	mv -f $@.tmp $@

test:
	cd tests && ${MAKE} test

clean:
	rm -f $O $B usage.msg
	cd tests && ${MAKE} clean
