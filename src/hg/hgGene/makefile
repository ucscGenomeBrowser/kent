include ../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  ${MYLIBDIR}/jkpbap.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgGene

O = hgGene.o altSplice.o dnaBindMotif.o domains.o \
    flyBaseInfo.o go.o links.o microarray.o \
    method.o localization.o mrnaDescriptions.o otherOrgs.o\
    pathways.o pseudoGene.o rnaStructure.o sequence.o swissProtComments.o

include ../../inc/cgi_build_rules.mk

my::
	rsync -ar --exclude=CVS/ --delete hgGeneData ${CGI_BIN}-${USER}/
	chmod -R a+rX ${CGI_BIN}-${USER}/hgGeneData

alpha::
	rm -fr ${CGI_BIN}/hgGeneData
	rsync -ar --exclude=CVS/ --delete hgGeneData ${CGI_BIN}/
	chmod -R g+w ${CGI_BIN}/hgGeneData
	chmod -R a+rX ${CGI_BIN}/hgGeneData

beta::
	rm -fr ${CGI_BIN}-beta/hgGeneData
	rsync -ar --exclude=CVS/ --delete hgGeneData ${CGI_BIN}-beta/
	chmod -R g+w ${CGI_BIN}-beta/hgGeneData
	chmod -R a+rX ${CGI_BIN}-beta/hgGeneData

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

test:
	${CGI_BIN}-${USER}/$A hgg_gene=D43968 hgg_chrom=chr21 hgg_start=35080500 hgg_end=35181383 db=hg16 hgsid=68172

