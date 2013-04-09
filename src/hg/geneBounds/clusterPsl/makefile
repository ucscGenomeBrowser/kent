kentSrc = ../../..
A = clusterPsl
include $(kentSrc)/inc/userApp.mk

test::
	clusterPsl -verbose=0 input.psl output.cluster
	diff expected.cluster output.cluster

clean::
	rm -f output.cluster
