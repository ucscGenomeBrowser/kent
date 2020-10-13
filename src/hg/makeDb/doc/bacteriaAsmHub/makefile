
## makefile definitions for the 'bacteria' assembly hubs

# these names trigger special behavior in the index page creation scripts:
Name=Bacteria
name=bacteria

destDir=/hive/data/genomes/asmHubs/${name}
srcDir=${HOME}/kent/src/hg/makeDb/doc/${name}AsmHub
orderList=$(CURDIR)/${name}.orderList.tsv

indexName=index
testIndexName=testIndex
statsName=asmStats
testStatsName=testAsmStats
dataName=trackData
testDataName=testTrackData
genomesTxt=genomes
hubFile=hub
testHubFile=testHub
hubTxtFile=hub.txt

# E. coli K-12 MG1655
defaultAssembly=GCF_000005845.2_ASM584v2

include ../asmHubs/asmHubs.mk
