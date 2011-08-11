# Build encode validator

include ../../../inc/common.mk

SCRIPT_FILES = \
	doEncodeValidate.pl
CONFIG_FILES = \
	fields.ra \
	labs.ra

CV = cv.ra
CVDIR=../../makeDb/trackDb/cv/alpha

PIPELINE_PATH=/hive/groups/encode/dcc/pipeline
CONFIG_DIR = ${PIPELINE_PATH}/${PIPELINE_DIR}/config
ENCODEDCC_DIR = ${PIPELINE_PATH}/downloads/encodeDCC

all:
	@${MAKE} --no-print-directory SCRIPTS=${SCRIPTS} scripts

development:
	@${MAKE} --no-print-directory  SCRIPTS=${PIPELINE_PATH}/encpipeline_${USER}/bin/scripts PIPELINE_DIR=encpipeline_${USER} scripts config

beta:
	@${MAKE} --no-print-directory SCRIPTS=${PIPELINE_PATH}/encpipeline_beta/bin/scripts PIPELINE_DIR=encpipeline_beta scripts config 

prod:
	@${MAKE} --no-print-directory SCRIPTS=${PIPELINE_PATH}/encpipeline_prod/bin/scripts PIPELINE_DIR=encpipeline_prod scripts config 
	cp -p ${CVDIR}/${CV} ${ENCODEDCC_DIR}/cv.ra.tmp
	mv -f  ${ENCODEDCC_DIR}/cv.ra.tmp ${ENCODEDCC_DIR}/cv.ra

# recursive targets, arguments:
#	SCRIPTS PIPELINE_DIR

scripts: ${SCRIPT_FILES:%=${SCRIPTS}/%}

${SCRIPTS}/%: %
	@mkdir -p $(dir $@)
	chmod +x $<
	perl -c $< 2>&1
	cp -p $< $@.tmp
	mv -f $@.tmp $@

config: ${CONFIG_FILES:%=${CONFIG_DIR}/%} ${CONFIG_DIR}/${CV}

${CONFIG_DIR}/${CV}: ${CVDIR}/${CV}
	cp -p ${CVDIR}/${CV} ${CONFIG_DIR}/cv.ra.tmp
	mv -f  ${CONFIG_DIR}/cv.ra.tmp ${CONFIG_DIR}/cv.ra

${CONFIG_DIR}/%: config/%
	@mkdir -p $(dir $@)
	cp -p $< $@.tmp
	mv -f $@.tmp $@

test:
	cd test && ${MAKE}

clean::
	echo nothing to clean in hg/encode/encodeValidate/
