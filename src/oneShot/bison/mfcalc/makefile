
mfcalc: mfcalc.c calc.c
	gcc -o mfcalc mfcalc.c yy.c calc.c -lm

mfcalc.c: mfcalc.y
	bison -d -o mfcalc.c mfcalc.y

clean::
	rm -f mfcalc mfcalc.c
