#!/bin/csh -ef
#
# doNhgriAssembly.csh - create NHGRI reports of ENCODE experiments from 'full' format for a single assembly
#
# usage: doNhgriAssembly.csh <db> <file>

set db = $1
set file = $2

echo "Project	Lab	Assay	Data Type	Experimental Factor	Organism	Cell Line	Strain	Tissue	Age	Treatment	Date Data Submitted	Release Date	Status	Submission ID	GEO/SRA IDs	Assembly	DCC Exp ID	DCC Accession	Lab Exp IDs" > $file.nhgri.tmp
tail -n +2 $file | \
    awk -v DB=$db -F"\t" '$15==DB {if (DB=="mm9") ORG = "mouse"; else ORG = "human" ; printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", \
        $2, $4, $5, "", $8, ORG, $6, $16, $23, $17, $18, $10, $11, $12, $13, $14, $15, $19, $20, $21)}' | sed 's/expanded/loaded/' >> $file.nhgri.tmp
grep 'Project' $file.nhgri.tmp > $file.nhgri.txt
grep -v 'Project' $file.nhgri.tmp | sort >> $file.nhgri.txt
cat $file.nhgri.txt
rm $file.nhgri.txt
