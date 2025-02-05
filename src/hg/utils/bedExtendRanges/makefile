kentSrc = ../../..
A =  bedExtendRanges
include ${kentSrc}/inc/userApp.mk

GOODFILES= bed7.txt bed8.txt chromBoundaries.txt
test:: good


good:
	@for good in $(GOODFILES); do \
		echo "Processing test file: $$good"; \
		${A} hg18 259 stdin < tests/$$good; \
	done
