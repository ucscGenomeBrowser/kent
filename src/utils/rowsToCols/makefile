kentSrc = ../..
A = rowsToCols
include $(kentSrc)/inc/userApp.mk
L += -lm

clean::
	rm -fr test/out

tests:
	${MKDIR} test/out
	rowsToCols test/in/spaced test/out/spaced
	diff test/expected/spaced test/out/spaced
	rowsToCols -varCol test/in/spacedVarious test/out/spacedVarious
	diff test/expected/spacedVarious test/out/spacedVarious
	rowsToCols -tab test/in/tab test/out/tab
	diff test/expected/tab test/out/tab
	rowsToCols -tab -varCol test/in/tabVarious test/out/tabVarious
	diff test/expected/tabVarious test/out/tabVarious
	rowsToCols -fs=, test/in/comma test/out/comma
	diff test/expected/comma test/out/comma
	rowsToCols -fs=, -varCol test/in/commaVarious test/out/commaVarious
	diff test/expected/commaVarious test/out/commaVarious
	rowsToCols -fixed test/in/fixed test/out/fixed
	diff test/expected/fixed test/out/fixed
