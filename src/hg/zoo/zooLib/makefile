.c.o:
	gcc -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc  -I../zooInc -c $*.c

O = misc.o util.o read_malign.o args.o

$(MACHTYPE)/zoo.a: $(O) $(MACHTYPE)
	ar rcu $(MACHTYPE)/zoo.a $(O)
	ranlib $(MACHTYPE)/zoo.a

$(MACHTYPE):
	mkdir $(MACHTYPE)

clean:
	rm -f ${O} $(MACHTYPE)/zoo.a
