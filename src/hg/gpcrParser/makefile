include ../../inc/common.mk

L += ${MYSQLLIBS} -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

#JKSRCDIR = ${HOME}/kent/src
#JKLIBDIR = ${JKSRCDIR}/lib/${MACHTYPE}
# MYLIBS =  ${JKLIBDIR}/jkhgap.a ${JKLIBDIR}/jkweb.a

O = gpcrParser.o

gpcrParser: $O 
	${CC} ${COPT} -o gpcrParser $O ${MYLIBS} ${L} 
	mv gpcrParser ${BINDIR}

clean::
	rm -f gpcrParser ${O}
