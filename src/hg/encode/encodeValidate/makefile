# Build encode validator

include ../../../inc/common.mk

SCRIPT_FILES = \
	doEncodeValidate.pl
CONFIG_FILES = \
	fields.ra \
	labs.ra \
	cv.ra

CV = cv.ra

PIPELINE_PATH=/cluster/data/encode/pipeline
CONFIG_DIR = ${PIPELINE_PATH}/${PIPELINE_DIR}/config

all:
	@${MAKE} --no-print-directory SCRIPTS=${HOME}/bin/scripts PIPELINE_DIR=encpipeline_${USER} scripts config

alpha:
	@${MAKE} --no-print-directory SCRIPTS=${SCRIPTS} PIPELINE_DIR=encpipeline_beta scripts config

install:
	@${MAKE} --no-print-directory SCRIPTS=${ENCODE_PIPELINE_BIN} PIPELINE_DIR=encpipeline_prod scripts config install-cgi

# recursive targets, arguments:
#   SCRIPTS PIPELINE_DIR

scripts: ${SCRIPT_FILES:%=${SCRIPTS}/%}

${SCRIPTS}/%: %
	@mkdir -p $(dir $@)
	chmod +x $<
	perl -c $<
	cp -p $< $@.tmp
	mv -f $@.tmp $@

config: ${CONFIG_FILES:%=${CONFIG_DIR}/%}

install-cgi:
	@if [ ! -d "${CGI_BIN}/encode" ]; then \
		${MKDIR} ${CGI_BIN}/encode; \
	fi
	rm -f ${CGI_BIN}/encode/${CV}
	cp -p config/${CV} ${CGI_BIN}/encode/${CV}

cgi:
	@if [ ! -d "${CGI_BIN}-${USER}/encode" ]; then \
		${MKDIR} ${CGI_BIN}-${USER}/encode; \
	fi
	rm -f ${CGI_BIN}-${USER}/encode/${CV}
	cp -p config/${CV} ${CGI_BIN}-${USER}/encode/${CV}

${CONFIG_DIR}/%: config/%
	@mkdir -p $(dir $@)
	cp -p $< $@.tmp
	mv -f $@.tmp $@

test:
	cd test && ${MAKE}
