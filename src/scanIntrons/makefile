include ../inc/common.mk

O = scanIntrons.o

scanIntrons.exe: $(O) lib
	gcc $O ../lib/$(MACHTYPE)/jkweb.a
	mv a.out scanIntrons.exe
	chmod a+rx scanIntrons.exe
	mv scanIntrons.exe ${CGI_BIN}-${USER}

lib: 
	cd ../lib;make; cd ../scanIntrons
