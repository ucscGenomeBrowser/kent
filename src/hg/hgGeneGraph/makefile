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
	rm -f ${CGI_BIN_USER}/hgGeneGraph
	cp -p hgGeneGraph ${CGI_BIN_USER}/

alpha::
	rm -f ${CGI_BIN}/hgGeneGraph
	cp -p hgGeneGraph ${CGI_BIN}/

beta::
	rm -f ${CGI_BIN_BETA}/hgGeneGraph
	cp -p hgGeneGraph ${CGI_BIN_BETA}/


clean::

compile::

lint::
	pylint -E hgGeneGraph

install::
