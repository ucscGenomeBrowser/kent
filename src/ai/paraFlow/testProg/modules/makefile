
test:
	mkdir -p output
	paraFlow oneCall/main.pf > output/oneCall
	diff expected/oneCall output/oneCall
	paraFlow crossRef/a.pf > output/crossRef
	diff expected/crossRef output/crossRef
