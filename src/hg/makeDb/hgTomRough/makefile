include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgTomRough.o tomRough.o

hgTomRough: $O 
	${CC} ${COPT} -o hgTomRough $O ${MYLIBS} $L

lib:
	cd ../../../lib && ${MAKE}

clean::
	rm -f hgTomRough ${O}
