
destDir=/hive/data/genomes/asmHubs/vgp
srcDir=${HOME}/kent/src/hg/makeDb/doc/vgpAsmHub
orderList=$(CURDIR)/vgp.primary.orderList.tsv

indexName=index
testIndexName=testIndex
statsName=asmStats
testStatsName=testAsmStats
dataName=trackData
testDataName=testTrackData
genomesTxt=primaryGenomes
hubFile=primaryHub
testHubFile=testPrimaryHub

Name=VGP
name=VGP

# GCA_009859065.2_bTaeGut2.pri.v2	zebra finch
defaultAssembly=GCA_009859065.2

include ../asmHubs/asmHubs.mk

userData:
	make -f userDataMakefile

alt:
	make -f altMakefile

trio:
	make -f trioMakefile

legacy:
	make -f legacyMakefile
