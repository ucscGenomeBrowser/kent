.c.o:
	gcc -I../../inc -c $*.c

chrom2gene: chrom2gene.o
	gcc -o chrom2gene chrom2gene.o ../../lib/$(MACHTYPE)/jkweb.a
