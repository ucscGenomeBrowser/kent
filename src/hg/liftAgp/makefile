
L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

%Test: %Test.c 
	gcc -Wimplicit -ggdb  -I../inc -I../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(L)
%: %.c 
	gcc -Wimplicit -ggdb  -I../inc -I../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(L)
	cp $@ $(HOME)/bin/$(ARCHTYPE)/

O = liftAgp

TESTS = 

exe: $(O)

test: $(TESTS)

lib:
	cd ../../lib; make
