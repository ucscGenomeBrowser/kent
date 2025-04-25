kentSrc = ../..
include $(kentSrc)/inc/common.mk

ifeq (${CGI_BIN_USER},)
    CGI_BIN_USER=${CGI_BIN}-${USER}
endif

ifeq (${CGI_BIN_BETA},)
    CGI_BIN_BETA=${CGI_BIN}-beta
endif

my::
	cp -p ${CPREMDESTOPT} gar.cgi.pl $(CGI_BIN_USER)/gar
	cp -p ${CPREMDESTOPT} asr.cgi.pl $(CGI_BIN_USER)/asr

cgi::
	cp -p ${CPREMDESTOPT} gar.cgi.pl $(CGI_BIN_USER)/gar
	cp -p ${CPREMDESTOPT} asr.cgi.pl $(CGI_BIN_USER)/asr

alpha::
	cp -p ${CPREMDESTOPT} gar.cgi.pl $(CGI_BIN)/gar
	cp -p ${CPREMDESTOPT} asr.cgi.pl $(CGI_BIN)/asr

beta::
	cp -p ${CPREMDESTOPT} gar.cgi.pl $(CGI_BIN_BETA)/gar
	cp -p ${CPREMDESTOPT} asr.cgi.pl $(CGI_BIN_BETA)/asr

install::
	cp -p ${CPREMDESTOPT} gar.cgi.pl $(CGI_BIN)/gar
	cp -p ${CPREMDESTOPT} asr.cgi.pl $(CGI_BIN)/asr

compile::
	@ls -og gar.cgi.pl
	@ls -og asr.cgi.pl

clean::
