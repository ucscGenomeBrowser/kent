include ../../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = joinerCheck.o 

joinerCheck: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/joinerCheck $O ${MYLIBS} $L
#	${STRIP} ${HOME}/bin/${MACHTYPE}/joinerCheck${EXE}

clean:
	rm -f $O

test: 
	joinerCheck ../all.joiner

foo:
	joinerCheck -fieldListIn=hgwdev.fields -identifier=swissProtId -database=hg16 ../all.joiner

ensembl:
	joinerCheck -fieldListIn=hgwbeta.fields -identifier=ensemblTranscriptId ../all.joiner -keys -fields -database=hg16
