JKDIR = /cse/guests/kent

FDIR = ../features/genie
G = $(FDIR)/genes.gdf
C = $(FDIR)/c2g
I = $(FDIR)/genes.ix

XDIR = $(JKDIR)/bin/sparc
X = $(XDIR)/geniegff
Y = $(XDIR)/indexgl
Z = $(XDIR)/makepgo

ENVSET = env JKWEB=$(JKDIR)/.html/cgi-bin/

$I: $G $Y
	$(ENVSET) $Y $G $I

$G: I.gff II.gff III.gff IV.gff V.gff X.gff $X
	$X $G $C
	$(ENVSET) $Z $C $(FDIR)/ .pgo
