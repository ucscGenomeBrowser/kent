include ../../inc/common.mk


O = countChars.o 

countChars: $O
	gcc -o $(HOME)/bin/$(MACHTYPE)/countChars $O ../../lib/$(MACHTYPE)/jkweb.a 
	strip $(HOME)/bin/$(MACHTYPE)/countChars

