kentSrc = ../../../..
A = hgGtex
include ${kentSrc}/inc/userApp.mk

dataDir = /hive/data/outside/GTEx/2014-01-17

test::
	${MKDIR} output

	hgGtex -tab=output -noLoad -limit=3 gtexTest ${dataDir}/GTEx_Analysis_2014-01-17_RNA-seq_RNA-SeQCv1.1.8_gene_rpkm.gct ${dataDir}/GTEx_Data_2014-01-17_Annotations_SampleAttributesDS.txt
	diff -r expected output

clean::
	rm -fr output
