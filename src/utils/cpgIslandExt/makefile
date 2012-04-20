include ../../inc/common.mk

A = cpglh
O = cpg_lh.o readseq.o

${A}: ${O}
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} $O

install::
	${STRIP} ${A}${EXE}
	rm -f ${DESTDIR}${BINDIR}/${A}${EXE}
	cp -p ${A}${EXE} ${DESTDIR}${BINDIR}/${A}${EXE}

clean::
	rm -f ${O} ${A}${EXE}

compile:: ${O}
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} $O

# compile:
# 	gcc readseq.c cpg_lh.c -o cpglh.exe
