
test:
	mkdir -p output
	paraFlow basic/basic.pf basic/in.basic > output/basic
	diff expected/basic expected/basic
	paraFlow crossRef/a.pf > output/crossRef
	diff expected/crossRef output/crossRef
	paraFlow dumpEnv/dumpEnv.pf > /dev/null
	paraFlow lib/testLib.pf > output/testLib
	diff expected/testLib output/testLib
	rm -f myExp/*.pfh
	paraFlow myExp/myExp.pf myExp/testExp.in > output/myExp
	diff expected/myExp output/myExp
	paraFlow myExp/myExp.pf myExp/testExp.in > output/myExp
	diff expected/myExp output/myExp
	rm -f oneCall/*.pfh
	paraFlow oneCall/main.pf > output/oneCall
	diff expected/oneCall output/oneCall
	paraFlow oneCall/main.pf > output/oneCall
	diff expected/oneCall output/oneCall
	paraFlow streamInterface/testStream.pf > output/streamInterface
	diff expected/streamInterface output/streamInterface
