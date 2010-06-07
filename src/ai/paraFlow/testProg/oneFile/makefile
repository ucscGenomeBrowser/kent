
test:
	mkdir -p output
	paraFlow serialMath.pf > output/serialMath
	diff expected/serialMath output/serialMath
	paraFlow houses.pf > output/houses
	diff expected/houses output/houses
	paraFlow interface.pf > output/interface
	diff expected/interface output/interface
	paraFlow namedParam.pf > output/namedParam
	diff expected/namedParam output/namedParam
	paraFlow nestedClass.pf > output/nestedClass
	diff expected/nestedClass output/nestedClass
	paraFlow paraAll.pf > output/paraAll
	diff expected/paraAll output/paraAll
	paraFlow paraMore.pf > output/paraMore
	diff expected/paraMore output/paraMore
	paraFlow paraTwoD.pf > output/paraTwoD
	diff expected/paraTwoD output/paraTwoD
	paraFlow printRef.pf > output/printRef
	diff expected/printRef output/printRef
	paraFlow printScan.pf foo > output/printScan
	diff expected/printScan output/printScan
	paraFlow tryCatch.pf foo > output/tryCatch
	diff expected/tryCatch output/tryCatch
	paraFlow wordCount.pf wordCount.pf serialMath.pf > output/wordCount
	diff expected/wordCount output/wordCount
	paraFlow wordUse.pf serialMath.pf > output/wordUse
	diff expected/wordUse output/wordUse
