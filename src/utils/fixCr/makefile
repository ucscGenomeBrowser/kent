include ../../inc/common.mk


O = fixCr.o 

fixCr: $O
	gcc -o $(HOME)/bin/$(MACHTYPE)/fixCr $O ../../lib/$(MACHTYPE)/jkweb.a 
	strip $(HOME)/bin/$(MACHTYPE)/fixCr

