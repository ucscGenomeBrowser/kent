include ../../inc/common.mk


L = -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = uniqSize.o 

uniqSize: $(O)
	gcc -o uniqSize $O $(MYLIBS) $(L) 


