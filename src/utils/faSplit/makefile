F64 = -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE
.c.o:
	gcc -O ${F64} -ggdb -I../../inc -c $*.c

O = faSplit.o 

faSplit: $(O)
	gcc ${F64} -o $(HOME)/bin/$(MACHTYPE)/faSplit $O ../../lib/$(MACHTYPE)/jkweb.a


