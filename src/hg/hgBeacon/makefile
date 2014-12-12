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
	pylint -E hgBeacon
	mkdir -p ${CGI_BIN_USER}/hgBeacon
	rm -f ${CGI_BIN_USER}/hgBeacon/hgBeacon
	rm -f ${CGI_BIN_USER}/hgBeacon/bigBedToBed
	cp -p hgBeacon ${CGI_BIN_USER}/hgBeacon/
	cp ${BINDIR}/bigBedToBed ${CGI_BIN_USER}/hgBeacon/
beta::
	mkdir -p ${CGI_BIN_BETA}/hgBeacon
	rm -f ${CGI_BIN_BETA}/hgBeacon/hgBeacon
	rm -f ${CGI_BIN_BETA}/hgBeacon/bigBedToBed
	cp -p hgBeacon ${CGI_BIN_BETA}/hgBeacon/
	cp /cluster/bin/x86_64/bigBedToBed ${CGI_BIN_BETA}/hgBeacon/
alpha::
	mkdir -p ${CGI_BIN}/hgBeacon
	pylint -E hgBeacon
	rm -f ${CGI_BIN}/hgBeacon/hgBeacon
	rm -f ${CGI_BIN}/bigBedToBed
	cp -p hgBeacon ${CGI_BIN}/hgBeacon/
	cp ${BINDIR}/bigBedToBed ${CGI_BIN_BETA}/hgBeacon/
clean::

compile::

install::

