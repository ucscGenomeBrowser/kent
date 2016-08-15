kentSrc = ../../../..
include ../../../../inc/common.mk

ensScripts = ensGeneToGenePred ensemblDbImport ensemblDownload ensemblGetAll ensemblSuperfamily ensemblVegaMap ensemblExport geneCheckAndLoad hgLoadEnsembl ensemblLoad mkRandomGoldLift mkRandomNTLift

all: ${ensScripts:%=${DESTDIR}${BINDIR}/%}

${DESTDIR}${BINDIR}/%: %
	cp -f $< $@
	chmod a+rx $@

alpha: ${ensScripts:%=${SCRIPTS}/%}

${SCRIPTS}/%: %
	cp -f $< $@
	chmod a+rx $@

clean::
	@echo nothing to clean in src/hg/makeDb/outside/ensembl/
