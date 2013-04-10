kentSrc = ../../..
A = refreshNamedSessionCustomTracks
include ${kentSrc}/inc/userApp.mk

%.o: %.c
	${CC} -DCGI_BIN=\"${CGI_BIN}\" ${COPT} ${CFLAGS} ${HG_DEFS} ${HG_WARN} ${HG_INC} ${XINC} -o $@ -c $<

ifeq (${SYS},Darwin)
# refreshNamedSessionCustomTracks uses sigtimedwait, which is not available on OS/X, despite being POSIX
donothing:
	@echo "Warning: refreshNamedSessionCustomTracks does not compile on OS/X" >&2
endif

SCRIPT_FILES = refreshSledgeHammer.pl makeExclusionList.pl \
               refreshSessionCtFiles.csh refreshSessionCtFilesInner.csh

scripts:
	@for F in ${SCRIPT_FILES}; do \
	    chmod +x $$F; \
	    rm -f ${SCRIPTS}/$${F}; \
	    echo cp -p $$F ${SCRIPTS}; \
	    cp -p $$F ${SCRIPTS}; \
	 done
