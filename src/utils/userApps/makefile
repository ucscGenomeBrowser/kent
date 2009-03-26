include ../../inc/common.mk

CMD_LIST = fetchChromSizes
DOC_LIST = bigBedWigs.txt

all::
	@${MKDIR} "${DESTDIR}${BINDIR}"
	@for P in ${CMD_LIST}; do \
	    ( rm -f "${DESTDIR}${BINDIR}/$${P}" && \
		cp -p $${P} "${DESTDIR}${BINDIR}" && \
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

# "$Id: makefile,v 1.3 2009/03/26 23:20:38 hiram Exp $"
