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
	cp -p --remove-destination  asl.cgi.pl $(CGI_BIN_USER)/asl

cgi::
	cp -p --remove-destination  gar.cgi.pl $(CGI_BIN_USER)/gar
	cp -p --remove-destination  asl.cgi.pl $(CGI_BIN_USER)/asl

alpha::
	cp -p --remove-destination  gar.cgi.pl $(CGI_BIN)/gar
	cp -p --remove-destination  asl.cgi.pl $(CGI_BIN)/asl

beta::
	cp -p --remove-destination  gar.cgi.pl $(CGI_BIN_BETA)/gar
	cp -p --remove-destination  asl.cgi.pl $(CGI_BIN_BETA)/asl

install::
	cp -p --remove-destination  gar.cgi.pl $(CGI_BIN)/gar
	cp -p --remove-destination  asl.cgi.pl $(CGI_BIN)/asl

compile::
	@ls -og gar.cgi.pl
	@ls -og asl.cgi.pl

clean::
