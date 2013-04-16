kentSrc = ../../..
A = validateManifest
include $(kentSrc)/inc/userApp.mk

test:: $A
	validateManifest -dir=/hive/groups/encode/encode3/testSets/manifest1 -quickMd5sum

test2: $A
	rm -f /hive/groups/encode/encode3/testSets/manifest1/validated.txt
	validateManifest -dir=/hive/groups/encode/encode3/testSets/manifest1 -quickMd5sum

# note "-" before the command means you know it will have an error, but continue making
