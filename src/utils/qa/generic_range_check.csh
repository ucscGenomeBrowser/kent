#!/bin/tcsh
source `which qaConfig.csh`
# Script to process the Gene Sorter tables to check MIN/MAX size of
# certain fields.  Will also check for NULL and empty fields
# Based on Bob Kuhn's spgGene.csh
# Now Generic for all genomes
# 06-24-04 M. Chalup
# ---------

# 1 July 04 M. Chalup

# Check number of arguments
if ($#argv != 2) then
        echo "Usage: generic_range_check.csh <current_db_release> <table_list_file>"
        exit 1
endif

set current_dir = $cwd
set db=$argv[1]
set listf=$argv[2]

rm -f temp.txt
rm -f temp_processed.txt

echo "Current Release is "$db
echo "Current host is "$HOST
echo

# Print out table name as DESC

foreach table (`cat $listf`)
	hgsql -N -e "DESC $table" $db  | gawk '{ printf("%s ", "'$table'"); print  $0 }' >> temp.txt
	set totRows=`hgsql -N -e  'SELECT COUNT(*) FROM '$table'' $db`
	echo "total rowcount for "$table" = "$totRows
end

echo
echo "=================="
echo

# Use gawk to print out to temp_processed only table name, field name, field
#type

gawk '{ printf("%s %s %s\n", $1, $2, $3) }' temp.txt > temp_processed.txt

@ linenum=`wc -l "temp_processed.txt" | gawk '{ print $1 }'`

@ del_num = 0

        echo "Table           field name       null   empty              min              max"
        echo "-------------------------------------------------------------------------------"

# If you try to use foreach command with a multi-string per line
# file you will not get the incrementation you want
# Below is a rather ugly put functional way to loop through a table row
# entries

while ( $del_num != $linenum )

	if ( $del_num != 0) then
		set current_line=`sed '1,'$del_num'd' temp_processed.txt | head -1`
	else
		set current_line=`head -1 temp_processed.txt`
	endif 
	set col_type=`echo $current_line | gawk '{ print $3 }'`
	set col_id=`echo $current_line | gawk '{ print $2 }'`
	set table_id=`echo $current_line | gawk '{ print $1 }'`
	set cnt_null=`hgsql -N -e 'SELECT COUNT(*) from '$table_id' WHERE '$col_id' is NULL' $db`
	set cnt_empty=`hgsql -N -e 'SELECT COUNT(*) from '$table_id' WHERE '$col_id' = ""' $db`
	# If col_type is non numeric, set min and max values to na
	if ($col_type =~ varchar* || \
	$col_type =~ longblob || \
	$col_type =~ blob || \
	$col_type =~ enum* || \
	$col_type =~ text || \
	$col_type =~ char* || \
	$col_type =~ date || \
	$col_type =~ longtext ) then
                       set cnt_min="na"
                       set cnt_max="na"
	else
                       set cnt_min=`hgsql -N -e 'SELECT MIN('$col_id') FROM \
                       '$table_id'' $db`
                       set cnt_max=`hgsql -N -e 'SELECT MAX('$col_id') FROM \
                       '$table_id'' $db`
       endif
               echo "$table_id $col_id $cnt_null $cnt_empty $cnt_min $cnt_max" \
               | gawk '{ printf("%-16s %-13s %6d %7d %16s %16s\n", $1, $2, $3, $4, $5 , $6) }'	
	@ del_num ++ 
end

echo

# For those rows which are indexed, display 'SHOW INDEX' info

echo "*** Show index information for indexed columns"
echo

egrep 'MUL|PRI' temp.txt | gawk '{ print $1 }' | sort | uniq > temp_indexed_tables.txt
 
foreach table (`cat temp_indexed_tables.txt`)
        echo
        echo $table
        hgsql -t -e "SHOW INDEX FROM $table" $db
        echo "----------------------------------"
end

