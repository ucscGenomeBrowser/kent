kentSrc = ../..
include ../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgapcgi.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgGene

O = hgGene.o alleles.o altSplice.o ctd.o dnaBindMotif.o domains.o primers.o gad.o malaCards.o\
    flyBaseInfo.o gtex.o go.o info.o links.o microarray.o \
    method.o localization.o mrnaDescriptions.o otherOrgs.o\
    pathways.o pseudoGene.o rgdInfo.o rnaStructure.o sequence.o swissProtComments.o \
    synonym.o wikiTrack.o geneReviews.o gencodeSection.o

include ../../inc/cgi_build_rules.mk

my::
	rm -fr ${CGI_BIN}-${USER}/hgGeneData
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete hgGeneData ${CGI_BIN}-${USER}/
	chmod -R g+w ${CGI_BIN}-${USER}/hgGeneData
	chmod -R a+rX ${CGI_BIN}-${USER}/hgGeneData

install::
	rm -fr ${DESTDIR}${CGI_BIN}/hgGeneData
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete hgGeneData \
		${DESTDIR}${CGI_BIN}/
	chmod -R a+rX ${DESTDIR}${CGI_BIN}/hgGeneData

alpha::
	rm -fr ${CGI_BIN}/hgGeneData
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete hgGeneData ${CGI_BIN}/
	chmod -R g+w ${CGI_BIN}/hgGeneData
	chmod -R a+rX ${CGI_BIN}/hgGeneData

beta::
	rm -fr ${CGI_BIN}-beta/hgGeneData
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete hgGeneData ${CGI_BIN}-beta/
	chmod -R g+w ${CGI_BIN}-beta/hgGeneData
	chmod -R a+rX ${CGI_BIN}-beta/hgGeneData

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

test:
	${CGI_BIN}-${USER}/$A hgg_gene=D43968 hgg_chrom=chr21 hgg_start=35080500 hgg_end=35181383 db=hg16 hgsid=68172

