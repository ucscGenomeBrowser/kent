include ../../../../inc/common.mk
HG_WARN=${HG_WARN_ERR}

ensScripts = ensGeneToGenePred ensemblDbImport ensemblDownload ensemblGetAll ensemblSuperfamily ensemblVegaMap exportEnsembl geneCheckAndLoad hgLoadEnsembl loadEnsembl mkRandomGoldLift mkRandomNTLift

all: ${ensScripts:%=${BINDIR}/%}

${BINDIR}/%: %
	cp -f $< $@
	chmod a+rx $@

alpha: ${ensScripts:%=${SCRIPTS}/%}

${SCRIPTS}/%: %
	cp -f $< $@
	chmod a+rx $@


