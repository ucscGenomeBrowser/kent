include ../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgGene

O = hgGene.o altSplice.o domains.o go.o links.o microarray.o \
    mrnaDescriptions.o pathways.o sequence.o swissProtComments.o

include ../../inc/cgi_build_rules.mk

my::
	chmod -R a+r hgGeneData
	chmod a+rx hgGeneData
	find hgGeneData -type d -exec /bin/chmod -f a+rx {} \;
	rsync -ar --delete hgGeneData ${CGI_BIN}-${USER}/
	find ${CGI_BIN}-${USER}/hgGeneData -type d -name CVS | xargs rm -fr

alpha::
	rm -fr ${CGI_BIN}/hgGeneData
	rsync -ar --delete hgGeneData ${CGI_BIN}/
	find ${CGI_BIN}/hgGeneData -type d -name CVS | xargs rm -fr

beta::
	rm -fr ${CGI_BIN}-beta/hgGeneData
	rsync -ar --delete hgGeneData ${CGI_BIN}-beta/
	find ${CGI_BIN}-beta/hgGeneData -type d -name CVS | xargs rm -fr

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv a.out $A
	chmod a+rx $A

test:
	${CGI_BIN}-${USER}/$A hgg_gene=D26481
	
