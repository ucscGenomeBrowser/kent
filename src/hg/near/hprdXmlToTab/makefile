
JKSRCDIR = $(HOME)/kent/src
JKLIBDIR = $(JKSRCDIR)/lib/$(MACHTYPE)

EXE = hprdXmlToTab

.c.o:
	gcc -ggdb -Wimplicit -DDEBUG -Wall -I$(JKSRCDIR)/inc -I$(JKSRCDIR)/hg/inc -c $*.c

L = $(MYSQLLIBS) -lm
MYLIBS = $(JKLIBDIR)/jkhgap.a $(JKLIBDIR)/jkweb.a 


all: $(EXE)

makeXmlParser: single-fixed.xml
	autoDtd single-fixed.xml out.dtd out.stats -tree=out.tree -atree=out.atree
	autoXml out.dtd hprd

runXmlParser: hprdXmlToTab single-fixed.html
	hprdXmlToTab single-fixed.xml hprd.p2p hprdComplex.tab

test: hprd.p2p
	hgNetDist hprd.p2p hg18 humanHprdP2P -weighted -threshold=2 -sqlRemap="select distinct value, name from knownToHprd"


O = $(EXE).o

$(EXE): $O hprd.o
	gcc $O hprd.o $(MYLIBS) $L -o $(EXE)
	chmod a+rx $(EXE)
	strip $(EXE)
	cp $(EXE) $(HOME)/bin/$(MACHTYPE)

clean:
	rm -f *.o $(EXE) 

