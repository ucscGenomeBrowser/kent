
mfcalc: mfcalc.c calc.c
	gcc -o mfcalc mfcalc.c calc.c -lm

mfcalc.c: mfcalc.y
	bison -o mfcalc.c mfcalc.y

