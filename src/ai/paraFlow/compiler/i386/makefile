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
	paraFlow for3.pf > expected/for3
	paraFlow -asm for3.pf > output/for3
	diff expected/for3 output/for3
	paraFlow for4.pf > expected/for4
	paraFlow -asm for4.pf > output/for4
	diff expected/for4 output/for4
	paraFlow for5.pf > expected/for5
	paraFlow -asm for5.pf > output/for5
	diff expected/for5 output/for5
	paraFlow for.pf > expected/for
	paraFlow -asm for.pf > output/for
	diff expected/for output/for
	paraFlow if.pf > expected/if
	paraFlow -asm if.pf > output/if
	diff expected/if output/if
	paraFlow if2.pf > expected/if2
	paraFlow -asm if2.pf > output/if2
	diff expected/if2 output/if2
	paraFlow if3.pf > expected/if3
	paraFlow -asm if3.pf > output/if3
	diff expected/if3 output/if3
	paraFlow if4.pf > expected/if4
	paraFlow -asm if4.pf > output/if4
	diff expected/if4 output/if4
