include ../../inc/common.mk

COPT = -g
L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgHeatmap

O = hgHeatmap.o clickOnImage.o configure.o ispyFeatures.o sortFeatures.o configureFeatures.o userSettings.o mainPage.o hgChromGraph.o

include ../../inc/cgi_build_rules.mk

my::
	rsync -ar --exclude=CVS/ --delete hgHeatmapData ${CGI_BIN}-${USER}/
	chmod -R a+rX ${CGI_BIN}-${USER}/hgHeatmapData

alpha::
	rsync -ar --exclude=CVS/ --delete hgHeatmapData ${CGI_BIN}/
	chmod -R a+rX ${CGI_BIN}/hgHeatmapData

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

