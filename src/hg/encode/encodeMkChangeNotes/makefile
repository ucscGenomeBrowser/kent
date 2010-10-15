# Build encode loader

include ../../../inc/common.mk

SCRIPT_FILES = \
	encodeMkChangeNotes

PIPELINE_PATH=/hive/groups/encode/dcc/pipeline

all:
	@${MAKE} --no-print-directory SCRIPTS=${HOME}/bin/scripts scripts

alpha:
	@${MAKE} --no-print-directory SCRIPTS=${SCRIPTS} PIPELINE_DIR=encpipeline_beta scripts

install:
	@${MAKE} --no-print-directory SCRIPTS=${ENCODE_PIPELINE_BIN} PIPELINE_DIR=encpipeline_prod scripts

scripts: ${SCRIPT_FILES:%=${SCRIPTS}/%}

${SCRIPTS}/%: %
	@mkdir -p $(dir $@)
	chmod +x $<
	cp -p $< $@.tmp
	mv -f $@.tmp $@

clean::
	echo nothing to clean in hg/encode/encodeMkChangeNotes/
