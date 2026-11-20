kentSrc = ../../..
include ${kentSrc}/inc/common.mk

matrixNormalize = ${DESTBINDIR}/matrixNormalize

test: rowSum rowLength colSum colLength

rowSum:
	@${MKDIR} -p output
	${matrixNormalize} row sum input/test.in output/$@.out
	diff expected/$@.out output/$@.out

rowLength:
	@${MKDIR} -p output
	${matrixNormalize} row length input/test.in output/$@.out
	diff expected/$@.out output/$@.out

colSum:
	@${MKDIR} -p output
	${matrixNormalize} column sum input/test.in output/$@.out
	diff expected/$@.out output/$@.out

colLength:
	@${MKDIR} -p output
	${matrixNormalize} column length input/test.in output/$@.out
	diff expected/$@.out output/$@.out

clean:
	rm -rf output
