include ../../../inc/common.mk

# add the follow to makefiles to enable stronger warning checks
# HG_WARN = ${HG_WARN_ERR}

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = visiGeneLoad.o

visiGeneLoad: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/visiGeneLoad $O ${MYLIBS} $L
#	${STRIP} ${HOME}/bin/${MACHTYPE}/visiGeneLoad${EXE}

clean:
	rm -f $O
