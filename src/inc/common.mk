COPT= -O -ggdb

.c.o:
	gcc -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE ${COPT} -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc ${XINC} -c $*.c

