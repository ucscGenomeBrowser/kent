F64 = -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE
.c.o:
	gcc ${F64} -I../../inc -c $*.c

O = ccCp.o 

ccCp: $O
	gcc ${F64} -o $(HOME)/bin/$(MACHTYPE)/ccCp $O ../../lib/$(MACHTYPE)/jkweb.a 

#	gcc ${F64} -o /projects/compbio/experiments/hg/bin/$(MACHTYPE)/ccCp $O ../../lib/$(MACHTYPE)/jkweb.a 

