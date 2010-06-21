
rpcalc: rpcalc.c
	gcc -o rpcalc rpcalc.c -lm

rpcalc.c: rpcalc.y
	bison -o rpcalc.c rpcalc.y

clean::
	rm -f rpcalc rpcalc.c
