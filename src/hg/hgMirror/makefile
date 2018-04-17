kentSrc = ../..
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
	rm -f ${CGI_BIN_USER}/hgMirror
	cp -p hgMirror ${CGI_BIN_USER}/
	# can only delete temp files as the apache user
	curl -s "http://hgwdev-${USER}.soe.ucsc.edu/cgi-bin/hgMirror?reset=1" || true
beta::
	rm -f ${CGI_BIN_BETA}/hgMirror
	cp -p hgMirror ${CGI_BIN_BETA}/
alpha::
	rm -f ${CGI_BIN}/hgMirror
	cp -p hgMirror ${CGI_BIN}/
clean::

compile::

lint::
	pylint -E hgMirror

install::
	rm -f ${DESTDIR}${CGI_BIN}/hgMirror
	cp -p hgMirror ${DESTDIR}${CGI_BIN}/
