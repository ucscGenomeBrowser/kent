P=$(HOME)/bin/$(MACHTYPE)

X=

H=$(HOME)/.html


all: ra features cDNA xeno
	echo made all

ra: ra/good.txt
	echo made ra

features: features/c2c features/c2g.ix features/sanger/genes.ix \
	  features/genie/genes.ix features/i.coo features/sanger/i.pgo \
	  features/genie/i.pgo features/orf2gene
	echo made features

cDNA: cDNA/allcdna.ix cDNA/good.ali cDNA/good.ix cDNA/i.alx cDNA/i.cdo \
      cDNA/introns.txt
	echo made cdna

xeno: xeno/cbriggsae/all.ix xeno/cbriggsae/i.xao
	echo made xeno

cDNA/allcdna.ix: cDNA/allcdna.fa $(P)/indexfa$(X)
	indexfa cDNA/allcdna.fa cDNA/allcdna.ix

ra/good.txt: cDNA/allcdna.ix $(P)/refiAli$(X)
	refiAli ea/all.out cDNA/allcdna nt4 ra/good.txt  ra/bad.txt ra/cool.txt ra/err.txt 0 1000000 features/c2g

cDNA/good.ali: ra/good.txt $(P)/binGood$(X)
	binGood ra/good.txt cDNA/good.ali

cDNA/good.ix: cDNA/good.ali $(P)/ixali$(X)
	ixali cDNA/good.ali cDNA/good.ix

cDNA/i.alx: cDNA/good.ali $(P)/ali2alx$(X)
	ali2alx cDNA/good.ali cDNA/ 

cDNA/i.cdo: ra/good.txt $(P)/cdnaOff$(X)
	cdnaOff ra/good.txt cDNA/ 

cDNA/introns.txt: ra/good.txt features/c2g.ix features/c2c.ix $(P)/introns$(X)
	introns ra/good.txt cDNA/introns.gff cDNA/introns.txt obsolete/altintron.txt $(H)/intronerator/altsplice.raw.html cDNA/introns.fa

features/c2c.ix: features/c2c $(P)/ixword3$(X)
	ixword3 features/c2c features/c2c.ix

features/c2g: features/sanger/c2g ra/good.txt $(P)/cluster$(X)
	cluster ra/good.txt features/sanger/c2g features/c2g $(H)/intronerator/nameless.raw.html

features/c2g.ix: features/c2g $(P)/ixword3$(X)
	ixword3 features/c2g features/c2g.ix

features/i.coo: features/c2c $(P)/makepgo$(X)
	makepgo features/c2c features/ .coo

features/sanger/genes.ix: features/sanger/genes.gdf $(P)/indexgl$(X)
	indexgl features/sanger/genes.gdf features/sanger/genes.ix

features/genie/genes.ix: features/genie/genes.gdf $(P)/indexgl$(X)
	indexgl features/genie/genes.gdf features/genie/genes.ix

features/genie/genes.gdf: genie/I.gff
	cd genie; make

features/sanger/i.pgo: features/c2g $(P)/makepgo$(X)
	makepgo features/c2g features/sanger/ .pgo

features/genie/i.pgo: features/c2g $(P)/makepgo$(X)
	makepgo features/genie/c2g features/genie/ .pgo

features/orf2gene: features/sanger/syn cDNA/good.ix $(P)/moresyn$(X)
	moresyn features/sanger/syn features/syn features/orf2gene features/orfInfo


xeno/cbriggsae/all.ix: $(P)/ixxenost$(X)  xeno/cbriggsae/all.st
	ixxenost xeno/cbriggsae/all.st xeno/cbriggsae/all.ix

xeno/cbriggsae/i.xao: $(P)/stToXao$(X) xeno/cbriggsae/all.st
	stToXao xeno/cbriggsae/all.st xeno/cbriggsae/
