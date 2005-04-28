
test:
	mkdir -p output
	rm -f oneCall/*.pfh
	paraFlow oneCall/main.pf > output/oneCall
	diff expected/oneCall output/oneCall
	paraFlow oneCall/main.pf > output/oneCall
	diff expected/oneCall output/oneCall
	paraFlow crossRef/a.pf > output/crossRef
	diff expected/crossRef output/crossRef
	rm -f myExp/*.pfh
	paraFlow myExp/myExp.pf myExp/testExp.in > output/myExp
	diff expected/myExp output/myExp
	paraFlow myExp/myExp.pf myExp/testExp.in > output/myExp
	diff expected/myExp output/myExp
	paraFlow basic/basic.pf basic/in.basic > output/basic
	diff expected/basic expected/basic
