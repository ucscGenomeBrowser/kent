
test:
	mkdir -p out
	matrixNormalize row sum test.in out/rowSum.out
	diff expected/rowSum.out out/rowSum.out
	matrixNormalize row length test.in out/rowLength.out
	diff expected/rowLength.out out/rowLength.out
	matrixNormalize column sum test.in out/colSum.out
	diff expected/colSum.out out/colSum.out
	matrixNormalize column length test.in out/colLength.out
	diff expected/colLength.out out/colLength.out

clean:
	rm -rf out/
