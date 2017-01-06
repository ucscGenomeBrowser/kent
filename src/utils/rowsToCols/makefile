kentSrc = ../..
A = rowsToCols
include $(kentSrc)/inc/userApp.mk
L += -lm

clean::
	rm -fr tests/out

test::
	${MKDIR} tests/out
	rowsToCols tests/in/spaced tests/out/spaced
	diff tests/expected/spaced tests/out/spaced
	rowsToCols -varCol tests/in/spacedVarious tests/out/spacedVarious
	diff tests/expected/spacedVarious tests/out/spacedVarious
	rowsToCols -tab tests/in/tab tests/out/tab
	diff tests/expected/tab tests/out/tab
	rowsToCols -tab -varCol tests/in/tabVarious tests/out/tabVarious
	diff tests/expected/tabVarious tests/out/tabVarious
	rowsToCols -fs=, tests/in/comma tests/out/comma
	diff tests/expected/comma tests/out/comma
	rowsToCols -fs=, -varCol tests/in/commaVarious tests/out/commaVarious
	diff tests/expected/commaVarious tests/out/commaVarious
	rowsToCols -fixed tests/in/fixed tests/out/fixed
	diff tests/expected/fixed tests/out/fixed
