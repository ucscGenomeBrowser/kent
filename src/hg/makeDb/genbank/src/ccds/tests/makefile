GBROOT = ../../..
include ${GBROOT}/common.mk

ccdsImport = ${BINARCH}/ccdsImport

all: test

test:
	@${MKDIR} output
	${ccdsImport} input/testAccAll.txt output/testAccAll.gp output/testAccAll.ccds
	diff -u expected/testAccAll.gp output/testAccAll.gp
	diff -u expected/testAccAll.ccds output/testAccAll.ccds

clean:
	rm -rf output
