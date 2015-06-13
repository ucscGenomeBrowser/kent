include ../../inc/common.mk

#cannot use common include rules, don't know how to overwrite :: targets
#include ../../inc/cgi_build_rules.mk

ifeq (${CGI_BIN_USER},)
    CGI_BIN_USER=${CGI_BIN}-${USER}
endif

ifeq (${CGI_BIN_BETA},)
    CGI_BIN_BETA=${CGI_BIN}-beta
endif

my::
	rm -f ${CGI_BIN_USER}/hgMenubar
	cp -p hgMenubar ${CGI_BIN_USER}/
beta::
	rm -f ${CGI_BIN_BETA}/hgMenubar
	cp -p hgMenubar ${CGI_BIN_BETA}/
alpha::
	rm -f ${CGI_BIN}/hgMenubar
	cp -p hgMenubar ${CGI_BIN}/
clean::

compile::

install::
	rm -f ${DESTDIR}${CGI_BIN}/hgMenubar
	cp -p hgMenubar ${DESTDIR}${CGI_BIN}/
