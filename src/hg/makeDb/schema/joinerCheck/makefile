include ../../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = joinerCheck.o 

joinerCheck: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/joinerCheck $O ${MYLIBS} $L
#	strip ${HOME}/bin/${MACHTYPE}/joinerCheck

clean:
	rm -f $O

test: 
	joinerCheck -parseOnly ../all.joiner

foo:
	joinerCheck -fieldListIn=hgwdev.fields -identifier=swissProtId -database=hg16 ../all.joiner

ensembl:
	joinerCheck -fieldListIn=hgwbeta.fields -identifier=ensemblTranscriptId ../all.joiner -keys -noTableCoverage -noDbCoverage -noTimes -database=hg16
