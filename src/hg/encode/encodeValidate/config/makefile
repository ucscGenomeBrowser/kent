#build the config directory for a pipeline

include ../../../../inc/common.mk


development:
	@${MAKE} --no-print-directory SCRIPTS=${PIPELINE_PATH}/encpipeline_${USER}/bin/scripts PIPELINE_DIR=encpipeline_${USER} config

beta:
	@${MAKE} --no-print-directory SCRIPTS=${PIPELINE_PATH}/encpipeline_beta/bin/scripts PIPELINE_DIR=encpipeline_beta config 

prod:
	@${MAKE} --no-print-directory SCRIPTS=${PIPELINE_PATH}/encpipeline_prod/bin/scripts PIPELINE_DIR=encpipeline_prod config 


config: ${CONFIG_FILES:%=${CONFIG_DIR}/%} ${CONFIG_DIR}/${CV}

${CONFIG_DIR}/${CV}: ${CVDIR}/${CV}
	cp -p ${CVDIR}/${CV} ${CONFIG_DIR}/cv.ra.tmp
	mv -f  ${CONFIG_DIR}/cv.ra.tmp ${CONFIG_DIR}/cv.ra

${CONFIG_DIR}/%: %
	cp -p $< $@.tmp
	mv -f $@.tmp $@
