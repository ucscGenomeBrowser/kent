include ../../../../inc/common.mk

#PROG = ${SCRIPTS}/doEncodeValidate.pl
PROG = ../doEncodeValidate.pl

all: test

test: dnase

dnase: output
	${PROG} x input/dnase -configDir=../../../config -outDir=output/dnase
	diff output/dnase/validateWig.out expected/dnase
	diff output/dnase/validateBed.out expected/dnase
	diff output/dnase/load.ra expected/dnase

output:
	${MKDIR} output/dnase

clean:
	rm -rf output
