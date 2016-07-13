kentSrc = ../../..
include ../../../inc/common.mk

O = misc.o util.o read_malign.o args.o

$(MACHTYPE)/zoo.a: $(O) $(MACHTYPE)
	ar rcu $(MACHTYPE)/zoo.a $(O)
	ranlib $(MACHTYPE)/zoo.a

$(MACHTYPE):
	mkdir $(MACHTYPE)

clean:
	rm -f ${O} $(MACHTYPE)/zoo.a
