kentSrc = ../../..
include ${kentSrc}/inc/common.mk

tabFmt = ${DESTBINDIR}/tabFmt

test:: defaultTest rightTest numRightTest specialTest passNoTabTest numRightPassNoTabTest

defaultTest: mkdirs
	${tabFmt} input/basic.tab >output/$@.txt
	diff expected/$@.txt output/$@.txt

rightTest: mkdirs
	${tabFmt} -right input/basic.tab >output/$@.txt
	diff expected/$@.txt output/$@.txt

numRightTest: mkdirs
	${tabFmt} -numRight input/basic.tab >output/$@.txt
	diff expected/$@.txt output/$@.txt

# special character test
specialTest: mkdirs
	${tabFmt} -numRight input/special.tab >output/$@.txt
	diff expected/$@.txt output/$@.txt

# test of -passNoTab
passNoTabTest: mkdirs
	${tabFmt} -numRight -passNoTab input/statsLabels.tab output/$@.txt
	diff expected/$@.txt output/$@.txt

# another tests of-numRight and -passNoTab that failed
numRightPassNoTabTest:
	${tabFmt} -numRight -passNoTab input/numRightPassNoTab.tab output/$@.txt
	diff expected/$@.txt output/$@.txt

mkdirs:
	@mkdir -p output

clean::
	rm -rf output
