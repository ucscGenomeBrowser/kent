#!/bin/csh -ef
#
# doNhgriReport.csh - create NHGRI format (quarterly) reports of ENCODE experiments from 'full' format for a single assembly
#
# usage: doNhgriReport.csh <db> <file> <end-date>

set db = $1
set file = $2
set enddate = $3

echo "Project	Lab	Assay	Data Type	Experimental Factor	Organism	Cell Line	Strain	Tissue	Age	Treatment	Date Data Submitted	Release Date	Status	Submission ID	GEO/SRA IDs	Assembly	DCC Exp ID	DCC Accession	Lab Exp IDs" > $file.nhgri.tmp
tail -n +2 $file | \
    awk -F"\t" -v ENDDATE="${enddate}" -v DB=$db '$15==DB {if (DB=="mm9") ORG = "mouse"; else ORG = "human"; end_time = ENDDATE"-23-59-59"; this_time = $10"-00-00-00"; end_time = gensub("-"," ", "g", end_time); this_time = gensub("-"," ", "g", this_time);  if (mktime(end_time) > mktime(this_time)) printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", \
        $2, $4, $5, "", $8, ORG, $6, $16, $23, $17, $18, $10, $11, $12, $13, $14, $15, $19, $20, $21)}' | sed 's/expanded/loaded/' >> $file.nhgri.tmp
grep 'Project' $file.nhgri.tmp > $file.nhgri.txt
grep -v 'Project' $file.nhgri.tmp | sort >> $file.nhgri.txt
cat $file.nhgri.txt
rm $file.nhgri.txt
