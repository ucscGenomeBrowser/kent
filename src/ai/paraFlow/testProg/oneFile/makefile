
test:
	mkdir -p output
	paraFlow serialMath.pf > output/serialMath
	diff expected/serialMath output/serialMath
	paraFlow houses.pf > output/houses
	diff expected/houses output/houses
	paraFlow wordCount.pf wordCount.pf serialMath > output/wordCount
	diff expected/wordCount output/wordCount
	paraFlow wordUse.pf serialMath.pf > output/wordUse
	diff expected/wordUse output/wordUse
