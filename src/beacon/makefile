include ../inc/common.mk

#cannot use common include rules, don't know how to overwrite :: targets
#include ../../inc/cgi_build_rules.mk

ifeq (${CGI_BIN_USER},)
    CGI_BIN_USER=${CGI_BIN}-${USER}
endif

ifeq (${CGI_BIN_BETA},)
    CGI_BIN_BETA=${CGI_BIN}-beta
endif

my::
	pylint -E query
	mkdir -p ${CGI_BIN_USER}/beacon
	rm -f ${CGI_BIN_USER}/beacon/query
	cp -p query ${CGI_BIN_USER}/beacon/
beta::
	pylint -E query
	mkdir -p ${CGI_BIN_BETA}/beacon
	rm -f ${CGI_BIN_BETA}/beacon/query
	cp -p query ${CGI_BIN_BETA}/beacon/
alpha::
	pylint -E query
	mkdir -p ${CGI_BIN}/beacon
	rm -f ${CGI_BIN}/beacon/query
	cp -p query ${CGI_BIN}/beacon/
clean::

compile::

install::
	mkdir -p ${DESTDIR}${CGI_BIN}/beacon
	rm -f ${DESTDIR}${CGI_BIN}/beacon/query
	cp -p query ${DESTDIR}${CGI_BIN}/
