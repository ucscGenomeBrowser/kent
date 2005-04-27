
test:
	mkdir -p output
	rm -f oneCall/*.pfh
	paraFlow oneCall/main.pf > output/oneCall
	diff expected/oneCall output/oneCall
	paraFlow oneCall/main.pf > output/oneCall
	diff expected/oneCall output/oneCall
	paraFlow crossRef/a.pf > output/crossRef
	diff expected/crossRef output/crossRef
	paraFlow myExp/myExp.pf myExp/testExp.in > output/myExp
	diff expected/myExp output/myExp
