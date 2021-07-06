kentSrc = ../../..
include ../../../inc/common.mk

PROG = bedJoinTabOffset

test:
	${PROG} in1.tab in.bed out1.bed
	@diff expectedOut1.bed out1.bed
	${PROG} -bedKey=3 in1.tab in.bed out2.bed
	@diff expectedOut1.bed out2.bed
	${PROG} -bedKey=4 in2.tab in.bed out3.bed
	@diff expectedOut2.bed out3.bed
	${MAKE} clean

clean:
	rm -f out1.bed out2.bed
