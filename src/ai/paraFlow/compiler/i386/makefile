test:
	mkdir -p expected output
	paraFlow crash.pf > expected/crash
	paraFlow -asm crash.pf > output/crash
	diff expected/crash output/crash
	paraFlow for1.pf > expected/for1
	paraFlow -asm for1.pf > output/for1
	diff expected/for1 output/for1
	paraFlow for2.pf > expected/for2
	paraFlow -asm for2.pf > output/for2
	diff expected/for2 output/for2
