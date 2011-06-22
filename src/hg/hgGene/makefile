include ../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  ${MYLIBDIR}/jkpbap.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgGene

O = hgGene.o altSplice.o ctd.o dnaBindMotif.o domains.o gad.o\
    flyBaseInfo.o go.o info.o links.o microarray.o \
    method.o localization.o mrnaDescriptions.o otherOrgs.o\
    pathways.o pseudoGene.o rgdInfo.o rnaStructure.o sequence.o swissProtComments.o \
    synonym.o wikiTrack.o

include ../../inc/cgi_build_rules.mk

my::
	rsync -aO --exclude=CVS/ --exclude=.svn/ --delete hgGeneData ${CGI_BIN}-${USER}/
	chmod -R a+rX ${CGI_BIN}-${USER}/hgGeneData

install::
	rm -fr ${DESTDIR}${CGI_BIN}/hgGeneData
	rsync -aO --exclude=CVS/ --exclude=.svn/ --delete hgGeneData \
		${DESTDIR}${CGI_BIN}/
	chmod -R a+rX ${DESTDIR}${CGI_BIN}/hgGeneData

alpha::
	rm -fr ${CGI_BIN}/hgGeneData
	rsync -aO --exclude=CVS/ --exclude=.svn/ --delete hgGeneData ${CGI_BIN}/
	chmod -R g+w ${CGI_BIN}/hgGeneData
	chmod -R a+rX ${CGI_BIN}/hgGeneData

beta::
	rm -fr ${CGI_BIN}-beta/hgGeneData
	rsync -aO --exclude=CVS/ --exclude=.svn/ --delete hgGeneData ${CGI_BIN}-beta/
	chmod -R g+w ${CGI_BIN}-beta/hgGeneData
	chmod -R a+rX ${CGI_BIN}-beta/hgGeneData

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

test:
	${CGI_BIN}-${USER}/$A hgg_gene=D43968 hgg_chrom=chr21 hgg_start=35080500 hgg_end=35181383 db=hg16 hgsid=68172

