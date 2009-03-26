include ../../inc/common.mk

INST_LIST = fetchChromSizes

all::
	@${MKDIR} "${DESTDIR}${BINDIR}"
	@for P in ${INST_LIST}; do \
	    ( rm -f "${DESTDIR}${BINDIR}/$${P}" && cp -p $${P} "${DESTDIR}${BINDIR}" && echo "installed $${P}" ) ; \
	done

# "$Id: makefile,v 1.1 2009/03/26 19:04:17 hiram Exp $"
