# Build encode validator

include ../../../inc/common.mk

SCRIPT_FILES = \
	doEncodeValidate.pl
CONFIG_FILES = \
	fields.ra \
	labs.ra \
	cv.ra

PIPELINE_PATH=/cluster/data/encode/pipeline
CONFIG_DIR = ${PIPELINE_PATH}/${PIPELINE_DIR}/config

all:
	@${MAKE} --no-print-directory SCRIPTS=${HOME}/bin/scripts PIPELINE_DIR=encpipeline_${USER} scripts config

alpha:
	@${MAKE} --no-print-directory SCRIPTS=${SCRIPTS} PIPELINE_DIR=encpipeline_beta scripts config

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

${CONFIG_DIR}/%: config/%
	@mkdir -p $(dir $@)
	cp -p $< $@.tmp
	mv -f $@.tmp $@

test:
	cd test && ${MAKE}
