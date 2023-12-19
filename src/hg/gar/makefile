kentSrc = ../..
include $(kentSrc)/inc/common.mk

ifeq (${CGI_BIN_USER},)
    CGI_BIN_USER=${CGI_BIN}-${USER}
endif

ifeq (${CGI_BIN_BETA},)
    CGI_BIN_BETA=${CGI_BIN}-beta
endif

my::
	cp -p --remove-destination  gar.cgi.pl $(CGI_BIN_USER)/gar

cgi::
	cp -p --remove-destination  gar.cgi.pl $(CGI_BIN_USER)/gar

alpha::
	cp -p --remove-destination  gar.cgi.pl $(CGI_BIN)/gar

beta::
	cp -p --remove-destination  gar.cgi.pl $(CGI_BIN_BETA)/gar

install::
	cp -p --remove-destination  gar.cgi.pl $(CGI_BIN)/gar

compile::
	@ls -og gar.cgi.pl

clean::
