kentSrc = ../../..
A = txCdsCluster
include ${kentSrc}/inc/userApp.mk

test::
	txCdsCluster input.bed output.clusters
	diff expected.clusters output.clusters
