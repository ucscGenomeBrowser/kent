F64 = -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE
.c.o:
	gcc -O -ggdb ${F64} -I../../inc -c $*.c

O = splitSql.o 

splitSql: $(O)
	gcc ${F64} -o $(HOME)/bin/$(MACHTYPE)/splitSql $O ../../lib/$(MACHTYPE)/jkweb.a


