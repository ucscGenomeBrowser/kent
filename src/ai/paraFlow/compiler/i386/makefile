test:
	mkdir -p expected output
	paraFlow crash.pf > expected/crash
	paraFlow -asm crash.pf > output/crash
	diff expected/crash output/crash
