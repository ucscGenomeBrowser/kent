# Build encode validator

include ../../../inc/common.mk

all:
	@${MAKE} --no-print-directory SCRIPTS=${SCRIPTS} scripts

#need to put int code to not allow non-existent encpiepline_foo to be created
development:
	@${MAKE} --no-print-directory SCRIPTS=${PIPELINE_PATH}/encpipeline_${USER}/bin/scripts PIPELINE_DIR=encpipeline_${USER} scripts
	cd config && ${MAKE} development

beta:
	@${MAKE} --no-print-directory SCRIPTS=${PIPELINE_PATH}/encpipeline_beta/bin/scripts PIPELINE_DIR=encpipeline_beta scripts 
	cd config && ${MAKE} beta

prod:
	@${MAKE} --no-print-directory SCRIPTS=${PIPELINE_PATH}/encpipeline_prod/bin/scripts PIPELINE_DIR=encpipeline_prod scripts 
	cd config && ${MAKE} prod
	cp -p ${CVDIR}/${CV} ${ENCODEDCC_DIR}/cv.ra.tmp
	mv -f  ${ENCODEDCC_DIR}/cv.ra.tmp ${ENCODEDCC_DIR}/cv.ra

#encpipeline_{pipeline}/bin/scripts needs to exist, otherwise the dev make could fail
scripts: ${SCRIPTS}/doEncodeValidate.pl

${SCRIPTS}/%: %
	chmod +x $<
	perl -c $< 2>&1
	cp -p $< $@.tmp
	mv -f $@.tmp $@

clean:
	echo nothing to clean in hg/encode/encodeValidate/
