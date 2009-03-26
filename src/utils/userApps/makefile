include ../../inc/common.mk

INST_LIST = fetchChromSizes

all::
	@${MKDIR} "${DESTDIR}${BINDIR}"
	@for P in ${INST_LIST}; do \
	    ( rm -f "${DESTDIR}${BINDIR}/$${P}" && \
		cp -p $${P} "${DESTDIR}${BINDIR}" && \
		chmod +x "${DESTDIR}${BINDIR}/$${P}" && \
		echo "installed ${DESTDIR}${BINDIR}/$${P}" ) ; \
	done

clean::
	@echo "nothing to clean in userApps"

# "$Id: makefile,v 1.2 2009/03/26 20:18:41 hiram Exp $"
