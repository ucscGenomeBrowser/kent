kentSrc = ../..
A = verticalSplitSqlTable
include $(kentSrc)/inc/userApp.mk
L += -lm

clean::
	rm -fr tests/out

test::
	verticalSplitSqlTable -mergeOk tests/in/antibody.tab tests/in/antibody.as tests/in/antibody.split tests/out
	diff tests/expected/cvDb_abTarget.as tests/out/cvDb_abTarget.as
	diff tests/expected/cvDb_abTarget.tab tests/out/cvDb_abTarget.tab
	diff tests/expected/cvDb_antibody.as tests/out/cvDb_antibody.as
	diff tests/expected/cvDb_antibody.tab tests/out/cvDb_antibody.tab
	diff tests/expected/mergeErrs.txt tests/out/mergeErrs.txt
