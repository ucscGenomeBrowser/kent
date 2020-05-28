
destDir=/hive/data/genomes/asmHubs/vgp
srcDir=${HOME}/kent/src/hg/makeDb/doc/vgpAsmHub
orderList=vgp.primary.orderList.tsv

indexName=index
testIndexName=testIndex
statsName=asmStats
testStatsName=testAsmStats
dataName=trackData
testDataName=testTrackData
genomesTxt=genomes
hubFile=hub
testHubFile=testHub

Name=VGP
name=vgp

# GCA_009859065.2_bTaeGut2.pri.v2	zebra finch
defaultAssembly=GCA_009859065.2

include ../asmHubs/asmHubs.mk

userData:
	make -f userDataMakefile

alt:
	make -f altMakefile

trio:
	make -f trioMakefile
