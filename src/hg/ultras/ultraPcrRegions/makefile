include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = ultraPcrRegions.o

ultraPcrRegions: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/ultraPcrRegions $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/ultraPcrRegions

clean:
	rm -f $O
