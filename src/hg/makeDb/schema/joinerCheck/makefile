kentSrc = ../../../..
A = joinerCheck
include ${kentSrc}/inc/userApp.mk

test::
	joinerCheck ../all.joiner

foo:
	joinerCheck -fieldListIn=hgwdev.fields -identifier=swissProtId -database=hg16 ../all.joiner

ensembl:
	joinerCheck -fieldListIn=hgwbeta.fields -identifier=ensemblTranscriptId ../all.joiner -keys -fields -database=hg16
