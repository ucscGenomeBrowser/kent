#!/bin/tcsh
source `which qaConfig.csh`
# Script to process the refLink table
# Based on Bob Kuhn's spgGene.csh 
# 04-28-04 M. Chalup
# ---------

# Check number of arguments
if ($#argv != 2) then
        echo "Usage: field_null_none_field_checker.csh <db_release> <table>"
        exit 1
endif

# tabl_a should contain the table with the strand info
set table_a=$argv[2]
set db=$argv[1]
set current_dir = $cwd

echo "Null analysis for "$table_a" for Release "$db 
echo
echo "get rowCounts and nullCounts:"
foreach table ($table_a)
	hgsql -N -e "DESC $table" $db | awk '{print $1}' > ${table}Cols
	echo $table
	echo "============="
	set totRows=`hgsql -N -e  'SELECT COUNT(*) FROM '$table'' $db`
	echo "total rows = "$totRows
	echo
	echo "null entries:"
	echo
	echo "Column         NULL    EMPTY"
	echo "----------------------------"
	foreach col (`cat ${table}Cols`)
		set cnt_a = `hgsql -N -e 'SELECT COUNT(*) from '$table' WHERE '$col' IS NULL' $db`
		set cnt_b = `hgsql -N -e 'SELECT COUNT(*) from '$table' WHERE '$col' = ""' $db`
	#	echo "$col\t$cnt_a\t$cnt_b"
	echo "$col $cnt_a $cnt_b" | gawk '{ printf("%-11s\t%d\t%d\n", $1, $2, $3) }'
	end
	echo
end
