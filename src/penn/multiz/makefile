CC=gcc
CFLAGS = -g -W -Wall -I../../inc

MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a 

multiz : multiz.c
	gcc $(CFLAGS) multiz.c yama.c scores.c util.c seq.c edit.c dna.c \
		$(MYLIBS) -o ../../../bin/$(MACHTYPE)/multiz

bak :
	cp multiz.c ../backup.d

tarball : README
	mkdir multiz-source
	cp README Makefile *.c *.h  multiz-source
	tar -cf - multiz-source | gzip > multiz.tgz
	rm -r multiz-source
