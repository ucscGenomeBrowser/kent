#!/bin/csh -vx
#
# doReport.csh - create DCC reports of ENCODE experiments
#                       report.<date>.dcc.txt   DCC format
#                       report.<date>.nhgri.txt NHGRI format
#                       report.<date>.all.txt   Has fields to create both formats

set reportDir = /hive/groups/encode/dcc/reports

cd /scratch/kate/build/kent/src/hg/makeDb/trackDb
make encodeReport
cd ~/kent/src/hg/encode/encodeValidate
cvs update config/cv.ra
cvs update config/pi.ra
set date = `date +%F`
set file = newreport.$date
./doEncodeReport.pl hg18 > $file.tmp
./doEncodeReport.pl hg19 >> $file.tmp
./doEncodeReport.pl mm9 >> $file.tmp

# replace certain data types with manual annotations, until trackDb entries for these 
# are corrected to better support reporting
grep -v Gencode $file.tmp | grep -v Mapability | grep -v BIP | grep -v NRE  > $file.tmp2
grep -v Project manual.all.txt >> $file.tmp2

# sort
grep 'Project' $file.tmp2 | head -1 > $file.all.txt
grep -v 'Project' $file.tmp2 | sort >> $file.all.txt

# create DCC report
awk -F"\t" '{printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",\
        $1, $3, $5, $6, $7, $9, $10, $11, $12, $15, $13)}' $file.all.txt > $file.dcc.tmp
grep 'Project' $file.dcc.tmp > $file.dcc.txt
grep -v 'Project' $file.dcc.tmp | sort >> $file.dcc.txt

# create NHGRI report
echo "Project	Lab	Assay	Data Type	Experimental Factor	Organism	Cell Line	Strain	Tissue	Stage/Treatment	Date Data Submitted	Release Date	Status	Submission ID	GEO/SRA IDs" > $file.nhgri.tmp
tail -n +2 $file.all.txt | \
    awk -F"\t" '{printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", \
        $2, $4, $5, "", $8, "human", $6, "n/a", "", $14, $10, $11, $12, $13, "")}' \
        | sed 's/	Control=None	/	none	/'>> $file.nhgri.tmp
grep 'Project' $file.nhgri.tmp > $file.nhgri.txt
grep -v 'Project' $file.nhgri.tmp | sort >> $file.nhgri.txt

cp $file.all.txt $reportDir
cp $file.{nhgri,dcc}.txt $reportDir
hgsql encpipeline_prod -e "select * from projects" > $reportDir/projects.$date.txt

# cleanup
rm $file.tmp $file.tmp2 
mv $file.*.txt ./reports/
cd reports
rm latest
ln -s $file.dcc.txt latest

