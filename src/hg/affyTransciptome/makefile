
L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a $(L)

O = affyPairsToSample

%: %.c 
	gcc -Wimplicit -ggdb  -I../inc -I../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS)
	cp $@ $(HOME)/bin/$(MACHTYPE)/

all : $(O)
