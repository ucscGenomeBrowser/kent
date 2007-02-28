include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 


%.o: %.c
	${CC} -DCGI_BIN=\"${CGI_BIN}\" ${COPT} ${CFLAGS} ${HG_DEFS} ${HG_WARN} ${HG_INC} ${XINC} -o $@ -c $<

O = refreshNamedSessionCustomTracks.o

refreshNamedSessionCustomTracks: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/refreshNamedSessionCustomTracks $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/refreshNamedSessionCustomTracks${EXE}

clean:
	rm -f $O

