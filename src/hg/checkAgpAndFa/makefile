include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkAgpAndFa.o

checkAgpAndFa: $O 
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/checkAgpAndFa $O $(MYLIBS) $L
	cp $(HOME)/bin/$(MACHTYPE)/checkAgpAndFa .

clean:
	rm *.o $(HOME)/bin/$(MACHTYPE)/checkAgpAndFa ./checkAgpAndFa
