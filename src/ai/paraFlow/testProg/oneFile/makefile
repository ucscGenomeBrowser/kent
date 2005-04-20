
test:
	mkdir -p output
	paraFlow serialMath.pf > output/serialMath.pf
	diff expected/serialMath.pf output/serialMath.pf
	paraFlow wordCount.pf wordCount.pf serialMath.pf > output/wordCount.pf
	diff expected/wordCount.pf output/wordCount.pf
