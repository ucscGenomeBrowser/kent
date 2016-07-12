kentSrc = ../../..
include ../../../inc/common.mk

liftAcross=${DESTBINDIR}/liftAcross

test::
	${liftAcross} -warn -bedOut=testOut.bed testAcross.lft \
		input.gp testOutput.gp 2> testErr.out
	diff testOutput.gp expectedOutput.gp
	diff testErr.out expectedErr.out
	diff testOut.bed expectedOut.bed

clean::
	rm -fr testOutput.gp testErr.out testOut.bed
