
ltcalc: ltcalc.c
	gcc -o ltcalc ltcalc.c -lm

ltcalc.c: ltcalc.y
	bison -o ltcalc.c ltcalc.y

clean::
	rm -f ltcalc ltcalc.c
