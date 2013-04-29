kentSrc = ../../..
A = knownToHprd
include $(kentSrc)/inc/userApp.mk

dbLoadTest::
	knownToHprd -verbose=2 -keepTab hg18 \
		/cluster/data/hg18/p2p/hprd/FLAT_FILES/HPRD_ID_MAPPINGS.txt

clean::
	rm -f knownToHprd.tab
