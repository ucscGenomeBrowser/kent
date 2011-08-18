#!/bin/tcsh
# BUILD $1 OMIM RELATED TRACKS

cat genemap|sed -e 's/|/\t/g' > genemap.tab

hgLoadSqlTab -warn $1 omimGeneMap ~/kent/src/hg/lib/omimGeneMap.sql genemap.tab

# Load mim2gene table

fgrep -v "gene/phenotype" mim2gene.txt > j.tmp.1
fgrep  "gene/phenotype" mim2gene.txt > j.tmp.2

cut -f 2 j.tmp.2|sed -e 's/\//\t/' >j.tmp

cut -f 1 j.tmp.2 >j.1

cut -f 1 j.tmp >j.g
cut -f 2 j.tmp >j.p

cut -f 3-5 j.tmp.2 >j.345

paste j.1 j.g j.345 >jj.g
paste j.1 j.p j.345 >jj.p

cat j.tmp.1 jj.g jj.p >mim2gene.updated.txt 

rm j.* jj.*


cut -f 1 mim2gene.updated.txt >j1
cut -f 2 mim2gene.updated.txt >j2
cut -f 3 mim2gene.updated.txt >j3

paste j1 j3 j2 >mim2gene.tab

hgsql $1 -e 'drop table mim2gene'
hgsql $1 < ~/kent/src/hg/lib/mim2gene.sql

hgsql $1 -e 'load data local infile "mim2gene.tab" into table mim2gene ignore 1 lines'


hgsql $1 -e 'drop table omim2gene'
hgsql $1 < ~/kent/src/hg/lib/omim2gene.sql

hgsql $1 -e 'load data local infile "mim2gene.updated.txt" into table omim2gene ignore 1 lines'

# build omimGeneSymbol table

doOmimGeneSymbols $1 j.out
cat j.out |sort -u >omimGeneSymbol.tab

hgLoadSqlTab -warn $1 omimGeneSymbol ~/kent/src/hg/lib/omimGeneSymbol.sql omimGeneSymbol.tab 

perl ./script1.pl --gene-map-file=genemap >omimPhenotype.tab

hgLoadSqlTab -warn $1 omimPhenotype ~/kent/src/hg/lib/omimPhenotype.sql omimPhenotype.tab 

hgsql $1 -e 'update omimPhenotype set omimPhenoMapKey = -1 where omimPhenoMapKey=0'
hgsql $1 -e 'update omimPhenotype set phenotypeId = -1 where phenotypeId=0'

doOmimGene2 $1 j.tmp
cat j.tmp |sort -u > omimGene2.tab

hgLoadBed $1 omimGene2 omimGene2.tab

rm j.tmp
##############################################################
# build the omimAvSnp track

mkdir -p av
cd av

# get the mimAV.txt data file from OMIM

cp ../mimAV.txt . -p

cut -f 1 mimAV.txt >j1
cut -f 2 mimAV.txt >j2
cut -f 3  mimAV.txt >j3
cut -f 4  mimAV.txt >j4
cut -f 5  mimAV.txt >j5

cat j1 |sed -e 's/\./\t/' >j1.2

cat j4 |sed -e 's/,/\t/' >j4-2
cut -f 1 j4-2 >j4.1
cut -f 2 j4-2 >j4.2

paste j1 j1.2 j3 j4 j4.1 j4.2 j5 j2 >omimAv.tab

hgsql $1 -e 'drop table omimAv'
hgsql $1 < ~/src/hg/lib/omimAv.sql
hgsql $1 -e 'load data local infile "omimAv.tab" into table omimAv ignore 1 lines'
hgsql $1 -e 'update omimAv set repl2 = rtrim(ltrim(repl2))'

doOmimAv $1 omimAvRepl.tab j.err

hgsql $1 -e "drop table omimAvRepl"
hgsql $1 < ~/kent/src/hg/lib/omimAvRepl.sql
hgsql $1 -e 'load data local infile "omimAvRepl.tab" into table omimAvRepl'

rm j1.2  j1 j2 j3  j4  j4-2  j4.1  j4.2  j5

if ($1 == "hg18") then
   hgsql $1 -N -e 'select chrom, chromStart, chromEnd, avId from omimAvRepl r, snp130 s where s.name = dbSnpId order by avId' |sort -u > omimAvSnp.tab
else
   hgsql $1 -N -e 'select chrom, chromStart, chromEnd, avId from omimAvRepl r, snp132 s where s.name = dbSnpId order by avId' |sort -u > omimAvSnp.tab
endif

hgLoadBed -allowStartEqualEnd  $1 omimAvSnp omimAvSnp.tab
cd ..

##############################################################
echo build omimLocation ...

mkdir -p location
cd location

doOmimLocation $1 omimLocation.bed j.err

hgLoadBed $1 omimLocation omimLocation.bed

# Remove all gene entries in omimGene2 from omimLocation table

hgsql $1 -N -e \
'delete from omimLocation where name  in (select name from omimGene2) '

# Per OMIM request, delete all the gray entries in omimLocation table.

mkdir -p cleanUpOmimLocation
cd cleanUpOmimLocation

echo cleaning omimLocation ...

hgsql $1 -N -e \
'select distinct name from omimLocation' |sort -u >j.all

hgsql $1 -N -e \
'select distinct name from omimLocation, omimPhenotype where name=omimId and omimPhenoMapKey=1' >j.1
hgsql $1 -N -e \
'select distinct name from omimLocation, omimPhenotype where name=omimId and omimPhenoMapKey=2' >j.2
hgsql $1 -N -e \
'select distinct name from omimLocation, omimPhenotype where name=omimId and omimPhenoMapKey=3' >j.3
hgsql $1 -N -e \
'select distinct name from omimLocation, omimPhenotype where name=omimId and omimPhenoMapKey=4' >j.4

cat j.1 j.2 j.3 j.4 |sort -u >j.1234

diff j.all j.1234 |grep "<" |sed -e "s/</do1 ${1}/" >doall

#cat doall

cat << '_EOF_' > do1
hgsql $1 -e "delete from omimLocation where name='${2}'"
'_EOF_'

#sleep 3
#echo after sleep

chmod +x do1
chmod +x doall
./doall

##############################################################

# remember to check in new mim2gene.sql

