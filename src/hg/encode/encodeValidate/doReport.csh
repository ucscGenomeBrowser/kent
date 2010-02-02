#!/bin/csh -vx

cd /scratch/kate/build/kent/src/hg/makeDb/trackDb
make encodeReport
cd ~/kent/src/hg/encode/encodeValidate
cvs update config/cv.ra
cvs update config/pi.ra
set date = `date +%F`
set file = report.$date
./doEncodeReport.pl > $file.tmp

# replace certain data types with manual annotations, until trackDb entries for these are corrected to better support reporting
grep -v Gencode $file.tmp | grep -v Mapability | grep -v BIP | grep -v NRE  > $file.tmp2
grep -v Project manual.all.txt >> $file.tmp2

# sort
grep 'Project' $file.tmp2 > $file.all.txt
grep -v 'Project' $file.tmp2 | sort >> $file.all.txt

# create DCC report
awk -F"\t" '{printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",\
        $1, $3, $5, $6, $7, $9, $10, $11, $12, $13)}' $file.all.txt > $file.dcc.tmp
grep 'Project' $file.dcc.tmp > $file.dcc.txt
grep -v 'Project' $file.dcc.tmp | sort >> $file.dcc.txt

# create NHGRI report
echo "Project	Lab	Assay	Data Type	Experimental Factor	Organism	Cell Line	Strain	Tissue	Stage/Treatment	Date Data Submitted	Release Date	Status	Submission ID	GEO/SRA IDs" > $file.nhgri.tmp
tail -n +2 $file.all.txt | \
    awk -F"\t" '{printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", \
        $2, $4, $5, "", $8, "human", $6, "n/a", "", "n/a", $10, $11, $12, $13, "")}' \
        | sed 's/	Control=None	/	none	/'>> $file.nhgri.tmp
grep 'Project' $file.nhgri.tmp > $file.nhgri.txt
grep -v 'Project' $file.nhgri.tmp | sort >> $file.nhgri.txt

set reportDir = /hive/groups/encode/dcc/reports
cp $file.all.txt $reportDir
cp $file.{nhgri,dcc}.txt $reportDir
hgsql encpipeline_prod -e "select * from projects" > $reportDir/projects.$date.txt
