include ../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgSpeciesRna.o

hgSpeciesRna: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/hgSpeciesRna $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/hgSpeciesRna

clean:
	rm -f $O
