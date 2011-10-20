include ../../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = joinerCheck.o 

joinerCheck: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/joinerCheck $O ${MYLIBS} $L
#	${STRIP} ${BINDIR}/joinerCheck${EXE}

clean:
	rm -f $O joinerCheck

test: 
	joinerCheck ../all.joiner

foo:
	joinerCheck -fieldListIn=hgwdev.fields -identifier=swissProtId -database=hg16 ../all.joiner

ensembl:
	joinerCheck -fieldListIn=hgwbeta.fields -identifier=ensemblTranscriptId ../all.joiner -keys -fields -database=hg16

compile:: $O ${MYLIBS}
	${CC} ${COPT} -o joinerCheck $O ${MYLIBS} $L
