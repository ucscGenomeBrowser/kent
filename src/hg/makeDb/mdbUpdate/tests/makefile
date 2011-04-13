include ../../../../inc/common.mk

all: expTest1 chipSeqTest1

UPDATE=mdbUpdate
PRINT=mdbPrint

TABLE=metaDbTest_${USER}

chipSeqTest1: setup
	${UPDATE} hg18 -recreate -table=${TABLE} input/chipSeqTest1.ra
	${PRINT} hg18 -table=${TABLE} -all > output/chipSeqTest1.ra
	diff output/chipSeqTest1.ra expected/

expTest1: setup
	${UPDATE} mm9 -recreate -table=${TABLE} input/smallTest.ra
	${PRINT} mm9 -table=${TABLE} -composite=wgEncodeTest -encodeExp > output/expTest1.ra
	diff output/expTest1.ra expected/

expTest2: setup
	${UPDATE} mm9 -recreate -table=${TABLE} input/smallTest.ra
	${PRINT} mm9 -table=${TABLE} -composite=wgEncodeTest -encodeExp > output/expTest1.ra
	diff output/expTest1.ra expected/

clean:
	rm -rf output

setup:
	${MAKE} clean
	@${MKDIR} output

