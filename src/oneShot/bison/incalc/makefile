
incalc: incalc.c
	gcc -o incalc incalc.c -lm

incalc.c: incalc.y
	bison -o incalc.c incalc.y

clean::
	rm -f incalc incalc.c
