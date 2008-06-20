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

SCRIPT_FILES = refreshSledgeHammer.pl makeExclusionList.pl

scripts:
	@for F in ${SCRIPT_FILES}; do \
	    chmod +x $$F; \
	    rm -f ${SCRIPTS}/$${F}; \
	    echo cp -p $$F ${SCRIPTS}; \
	    cp -p $$F ${SCRIPTS}; \
	 done

clean:
	rm -f $O

