include ../../inc/common.mk

L = ${MYSQLLIBS} -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

#JKSRCDIR = ${HOME}/kent/src
#JKLIBDIR = ${JKSRCDIR}/lib/${MACHTYPE}
# MYLIBS =  ${JKLIBDIR}/jkhgap.a ${JKLIBDIR}/jkweb.a

#.c.o:
#	${CC} ${COPT} ${CFLAGS} -Wimplicit -I${JKSRCDIR}/inc -I${JKSRCDIR}/hg/inc -c $*.c


O = gpcrParser.o

gpcrParser: $O 
	${CC} ${COPT} -o gpcrParser $O ${MYLIBS} ${L} 
	mv gpcrParser ${HOME}/bin/${MACHTYPE}

clean::
	rm -f gpcrParser ${O}
