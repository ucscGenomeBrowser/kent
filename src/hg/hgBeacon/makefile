include ../../inc/common.mk

#cannot use common include rules, don't know how to overwrite :: targets
#include ../../inc/cgi_build_rules.mk

ifeq (${CGI_BIN_USER},)
    CGI_BIN_USER=${CGI_BIN}-${USER}
endif

ifeq (${CGI_BIN_BETA},)
    CGI_BIN_BETA=${CGI_BIN}-beta
endif

PATH:=$(PATH):/cluster/software/bin

my::
	pylint -E --rcfile=pylint.rc hgBeacon
	rm -f ${CGI_BIN_USER}/hgBeacon
	mkdir -p ${CGI_BIN_USER}/beacon2
	ln -sf ${CGI_BIN_USER}/hgBeacon ${CGI_BIN_USER}/beacon2/info
	ln -sf ${CGI_BIN_USER}/hgBeacon ${CGI_BIN_USER}/beacon2/query
	cp -p hgBeacon ${CGI_BIN_USER}/
beta::
	pylint -E --rcfile=pylint.rc hgBeacon
	rm -f ${CGI_BIN_BETA}/hgBeacon
	cp -p hgBeacon ${CGI_BIN_BETA}/
alpha::
	pylint -E --rcfile=pylint.rc hgBeacon
	rm -f ${CGI_BIN}/hgBeacon
	cp -p hgBeacon ${CGI_BIN}/
clean::

compile::

install::

