include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = hgBedsToBedExps.o bToBeCfg.o

hgBedsToBedExps: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgBedsToBedExps $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgBedsToBedExps${EXE}

clean:
	rm -f $O

test:
	cd tests/first; hgBedsToBedExps -verbose=0 in.cfg out.bed out.exps
	cd tests/first; diff expected.exps out.exps
	cd tests/first; diff expected.bed out.bed
