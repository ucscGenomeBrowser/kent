kentSrc = ../..

include ${kentSrc}/inc/localEnvironment.mk
include ${kentSrc}/inc/common.mk

CMD_LIST = fetchChromSizes
DOC_LIST = 

all::
	@${MKDIR} "${DESTDIR}${BINDIR}"
	@for P in ${CMD_LIST}; do \
	    ( rm -f "${DESTDIR}${BINDIR}/$${P}" && \
		cp $${P} "${DESTDIR}${BINDIR}" && \
		chmod +x "${DESTDIR}${BINDIR}/$${P}" && \
		echo "installed ${DESTDIR}${BINDIR}/$${P}" ) ; \
	done
	@for P in ${DOC_LIST}; do \
	    ( rm -f "${DESTDIR}${BINDIR}/$${P}" && \
		cp -p $${P} "${DESTDIR}${BINDIR}" && \
		echo "installed ${DESTDIR}${BINDIR}/$${P}" ) ; \
	done

clean::
	@echo "nothing to clean in userApps"

# "$Id: makefile,v 1.5 2009/10/07 18:18:38 galt Exp $"
