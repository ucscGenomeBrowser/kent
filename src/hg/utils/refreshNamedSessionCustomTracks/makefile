include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 


%.o: %.c
	${CC} -DCGI_BIN=\"${CGI_BIN}\" ${COPT} ${CFLAGS} ${HG_DEFS} ${HG_WARN} ${HG_INC} ${XINC} -o $@ -c $<

O = refreshNamedSessionCustomTracks.o

ifeq (${SYS},Darwin)
# refreshNamedSessionCustomTracks uses sigtimedwait, which is not available on OS/X, despite being POSIX
donothing:
	@echo "Warning: refreshNamedSessionCustomTracks does not compile on OS/S" >&2
endif
refreshNamedSessionCustomTracks: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/refreshNamedSessionCustomTracks $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/refreshNamedSessionCustomTracks${EXE}

SCRIPT_FILES = refreshSledgeHammer.pl makeExclusionList.pl \
               refreshSessionCtFiles.csh refreshSessionCtFilesInner.csh

scripts:
	@for F in ${SCRIPT_FILES}; do \
	    chmod +x $$F; \
	    rm -f ${SCRIPTS}/$${F}; \
	    echo cp -p $$F ${SCRIPTS}; \
	    cp -p $$F ${SCRIPTS}; \
	 done

compile: ${O}
	${CC} ${COPT} -o refreshNamedSessionCustomTracks $O ${MYLIBS} $L

clean:
	rm -f $O refreshNamedSessionCustomTracks

