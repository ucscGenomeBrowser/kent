include ../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgGene

O = hgGene.o

include ../../inc/cgi_build_rules.mk

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv a.out $A
	chmod a+rx $A

test:
	${CGI_BIN}-${USER}/$A hgg_gene=D26481
	
