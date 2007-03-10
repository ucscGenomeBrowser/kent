include ../../../../inc/common.mk
HG_WARN=${HG_WARN_ERR}

ensScripts = ensGeneToGenePred ensemblDbImport ensemblDownload ensemblGetAll geneCheckAndLoad exportEnsembl loadEnsembl hgLoadEnsembl

all: ${ensScripts:%=${HOME}/bin/scripts/%}

${HOME}/bin/scripts/%: %
	cp -f $< $@
	chmod a+rx $@

alpha: ${ensScripts:%=${SCRIPTS}/%}

${SCRIPTS}/%: %
	cp -f $< $@
	chmod a+rx $@
