include ../../inc/common.mk


O = geneStarts.o 

L = $(MYSQLLIBS) -lm 
MYLIBDIR = /cse/guests/kent/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


geneStarts: $(O)
	gcc -o geneStarts $O $(MYLIBS) $L

