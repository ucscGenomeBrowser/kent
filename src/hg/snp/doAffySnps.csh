#!/bin/csh -fe

# rm -f log ; date ; ./doAffySnps.csh > & log ; date ; cat log

#exit;
echo
echo `date` starting writeBeds.csh
set db = hg17
cd /cluster/data/$db/bed/snp/affy/latest
touch affy.txt affy.bed Affy.bed bed.tab
rm -f affy*.txt affy*.bed Affy.bed* bed.tab

# datafile was provided by Valmeekam, Venu [Venu_Valmeekam@affymetrix.com]
tar xfz affyhg17maps_withstrand_alleles.tgz
wc -l affy*txt

echo
echo `date` writing files
awk '$1 !~ /^chrom/ {printf("chr%s\t%d\t%d\t%s\t0\t%s\t%s\tunknown\tsnp\tunknown\t0\t0\tunknown\texact\tAffy10K\t0\n",        $1,$2,$3,$4,$6,$7);}' < affy10K.txt         > affy10K.bed
awk '$1 !~ /^chrom/ {printf("chr%s\t%d\t%d\t%s\t0\t%s\t%s\tunknown\tsnp\tunknown\t0\t0\tunknown\texact\tAffy10Kv2\t0\n",      $1,$2,$3,$4,$6,$7);}' < affy10Kv2.txt       > affy10Kv2.bed
awk '$1 !~ /^chrom/ {printf("chr%s\t%d\t%d\t%s\t0\t%s\t%s\tunknown\tsnp\tunknown\t0\t0\tunknown\texact\tAffy50K_HindIII\t0\n",$1,$2,$3,$4,$6,$7);}' < affy50K_HindIII.txt > affy50K_HindIII.bed
awk '$1 !~ /^chrom/ {printf("chr%s\t%d\t%d\t%s\t0\t%s\t%s\tunknown\tsnp\tunknown\t0\t0\tunknown\texact\tAffy50K_XbaI\t0\n",   $1,$2,$3,$4,$6,$7);}' < affy50K_XbaI.txt    > affy50K_XbaI.bed

# this is a temporary kluge to fix some bad input data.
cat affy*.bed | sed 's/_par//' > Affy.bed

echo
echo `date` deleting old rows
# the source enum for 'dbSnp' is 2; all of the affy* values are higher.
hgsql $db -e "delete from snp where source > 2 "

echo
echo `date` loading table
hgLoadBed $db snp Affy.bed -oldTable -tab

rm -f affy*.txt affy*.bed bed.tab
gzip Affy.bed
echo
echo `date` finished writeBeds.csh
echo
