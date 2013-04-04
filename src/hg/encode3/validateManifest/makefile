kentSrc = ../../..
A = validateManifest
include $(kentSrc)/inc/userApp.mk

test::
	validateManifest -dir=/hive/groups/encode/encode3/testSets/manifest1

test2:
	rm -f /hive/groups/encode/encode3/testSets/manifest1/validated.txt
	validateManifest -dir=/hive/groups/encode/encode3/testSets/manifest1

# note "-" before the command means you know it will have an error, but continue making
