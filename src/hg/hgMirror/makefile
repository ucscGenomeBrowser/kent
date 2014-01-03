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
	cp hgMirror ${CGI_BIN_USER}/
beta::
	cp hgMirror ${CGI_BIN_BETA}/
alpha::
	cp hgMirror ${CGI_BIN}/
