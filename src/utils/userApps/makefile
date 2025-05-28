kentSrc = ../..

include ${kentSrc}/inc/localEnvironment.mk
include ${kentSrc}/inc/common.mk

CMD_LIST = fetchChromSizes

all:: ${CMD_LIST:%=${DESTDIR}${BINDIR}/%}

${DESTDIR}${BINDIR}/%: %
	@${MKDIR} "${DESTDIR}${BINDIR}"
	rm -f "${DESTDIR}${BINDIR}/$*"
	cp -p $* "${DESTDIR}${BINDIR}/"
	chmod +x "${DESTDIR}${BINDIR}/$*"


