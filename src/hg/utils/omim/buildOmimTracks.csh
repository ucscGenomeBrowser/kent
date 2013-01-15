#!/bin/tcsh -e
# BUILD $db OMIM RELATED TRACKS

set db=$1

cat genemap|sed -e 's/|/\t/g' > genemap.tab

hgLoadSqlTab -verbose=0 -warn $db omimGeneMapNew ~/kent/src/hg/lib/omimGeneMap.sql genemap.tab 

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

# BRIAN tail -n +2 mim2gene.tab | hgLoadSqlTab -verbose=0 -warn $db omim2geneNew ~/kent/src/hg/lib/omim2gene.sql stdin
#hgsql $db -e 'drop table mim2geneNew'
#hgsql $db < ~/kent/src/hg/lib/mim2gene.sql

#hgsql $db -e 'load data local infile "mim2gene.tab" into table mim2gene ignore 1 lines'


tail -n +2 mim2gene.updated.txt | hgLoadSqlTab -verbose=0 -warn $db omim2geneNew ~/kent/src/hg/lib/omim2gene.sql stdin 
# hgsql $db -e 'drop table omim2gene'
# hgsql $db < ~/kent/src/hg/lib/omim2gene.sql

# hgsql $db -e 'load data local infile "mim2gene.updated.txt" into table omim2gene ignore 1 lines'

# build omimGeneSymbol table

../../doOmimGeneSymbols $db j.out
cat j.out |sort -u >omimGeneSymbol.tab

hgLoadSqlTab -verbose=0 -warn $db omimGeneSymbolNew ~/kent/src/hg/lib/omimGeneSymbol.sql omimGeneSymbol.tab  

perl ./parseGeneMap.pl --gene-map-file=genemap >omimPhenotype.tab

hgLoadSqlTab -verbose=0 -warn $db omimPhenotypeNew ~/kent/src/hg/lib/omimPhenotype.sql omimPhenotype.tab  

hgsql $db -e 'update omimPhenotypeNew set omimPhenoMapKey = -1 where omimPhenoMapKey=0'
hgsql $db -e 'update omimPhenotypeNew set phenotypeId = -1 where phenotypeId=0'

../../doOmimGene2 $db j.tmp
cat j.tmp |sort -u > omimGene2.tab

hgLoadBed -verbose=0 $db omimGene2New omimGene2.tab 

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

tail -n +2  omimAv.tab | hgLoadSqlTab -verbose=0 -warn $db omimAvNew ~/kent/src/hg/lib/omimAv.sql stdin 
#hgsql $db -e 'drop table omimAv'
#hgsql $db < ~/src/hg/lib/omimAv.sql
#hgsql $db -e 'load data local infile "omimAv.tab" into table omimAv ignore 1 lines'

hgsql $db -e 'update omimAvNew set repl2 = rtrim(ltrim(repl2))'

../../../doOmimAv $db omimAvRepl.tab j.err

tail -n +2  omimAvRepl.tab | hgLoadSqlTab -verbose=0 -warn $db omimAvReplNew ~/kent/src/hg/lib/omimAvRepl.sql stdin 
# hgsql $db -e "drop table omimAvRepl"
# hgsql $db < ~/kent/src/hg/lib/omimAvRepl.sql
# hgsql $db -e 'load data local infile "omimAvRepl.tab" into table omimAvRepl'

rm j1.2  j1 j2 j3  j4  j4-2  j4.1  j4.2  j5

if ($db == "hg18") then
   hgsql $db -N -e 'select chrom, chromStart, chromEnd, avId from omimAvReplNew r, snp130 s where s.name = dbSnpId order by avId' |sort -u > omimAvSnp.tab
else
   hgsql $db -N -e 'select chrom, chromStart, chromEnd, avId from omimAvReplNew r, snp137 s where s.name = dbSnpId order by avId' |sort -u > omimAvSnp.tab
endif

hgLoadBed -verbose=0 -allowStartEqualEnd  $db omimAvSnpNew omimAvSnp.tab 
cd ..

##############################################################
echo build omimLocation ...

mkdir -p location
cd location

# ../../../doOmimLocation $db omimLocation.bed j.err
../../../doOmimLocation $db omimLocation.bed 

hgLoadBed -verbose=0 $db omimLocationNew omimLocation.bed 

# Remove all gene entries in omimGene2 from omimLocation table

hgsql $db -N -e \
'delete from omimLocationNew where name  in (select name from omimGene2New) '

# Per OMIM request, delete all the gray entries in omimLocation table.

mkdir -p cleanUpOmimLocation
cd cleanUpOmimLocation

echo cleaning omimLocation ...

hgsql $db -N -e \
'select distinct name from omimLocationNew' |sort -u >j.all

hgsql $db -N -e \
'select distinct name from omimLocationNew, omimPhenotypeNew where name=omimId and omimPhenoMapKey=1' >j.1
hgsql $db -N -e \
'select distinct name from omimLocationNew, omimPhenotypeNew where name=omimId and omimPhenoMapKey=2' >j.2
hgsql $db -N -e \
'select distinct name from omimLocationNew, omimPhenotypeNew where name=omimId and omimPhenoMapKey=3' >j.3
hgsql $db -N -e \
'select distinct name from omimLocationNew, omimPhenotypeNew where name=omimId and omimPhenoMapKey=4' >j.4

cat j.1 j.2 j.3 j.4 |sort -u >j.1234

join -v 1 j.all j.1234 |sed -e "s/^/.\/do1  /" >doall

#cat doall

cat << _EOF_ > do1
hgsql $db -e 'delete from omimLocationNew where name="\$2"'
_EOF_

#sleep 3
#echo after sleep

chmod +x do1
chmod +x doall
./doall

##############################################################

# remember to check in new mim2gene.sql

