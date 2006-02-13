
test:
	mkdir -p output
	paraFlow badDefault.pf > output/badDefault
	diff expected/badDefault output/badDefault
	paraFlow defaultAhead.pf > output/defaultAhead
	diff expected/defaultAhead output/defaultAhead
	paraFlow dirAddTuple.pf > output/dirAddTuple
	diff expected/dirAddTuple output/dirAddTuple
	paraFlow dotStringDotMethod.pf > output/dotStringDotMethod
	diff expected/dotStringDotMethod output/dotStringDotMethod
	paraFlow castMultiReturn.pf > output/castMultiReturn
	diff expected/castMultiReturn output/castMultiReturn
	paraFlow func2func2.pf > output/func2func2
	diff expected/func2func2 output/func2func2
	paraFlow func2in2class.pf > output/func2in2class
	diff expected/func2in2class output/func2in2class
	paraFlow memberDotInClass.pf > output/memberDotInClass
	diff expected/memberDotInClass output/memberDotInClass
	paraFlow nilStuff.pf > output/nilStuff
	diff expected/nilStuff output/nilStuff
	paraFlow oneIntoClass.pf oneIntoClass.pf > output/oneIntoClass
	diff expected/oneIntoClass output/oneIntoClass
