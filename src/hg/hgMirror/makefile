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
	rm -f ${CGI_BIN}/hgMirror
	cp -p hgMirror ${CGI_BIN}/
beta::
	rm -f ${CGI_BIN}/hgMirror
	cp -p hgMirror ${CGI_BIN}/
alpha::
	rm -f ${CGI_BIN}/hgMirror
	cp -p hgMirror ${CGI_BIN}/
clean::

compile::

install::
	rm -f ${DESTDIR}${CGI_BIN}/hgMirror
	cp -p hgMirror ${DESTDIR}${CGI_BIN}/
