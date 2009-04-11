#!/bin/tcsh
source `which qaConfig.csh`
# Script to test the chromInfo table
# 21 April 2004 M. Chalup

# Check number of arguments
if ($#argv != 1) then
        echo "Usage: chromInfo_file_test.csh <db_release>"
        exit 1
endif


set table_a = "chromInfo"
set db = $argv[1]
rm -f $db.$table_a.nibSize_output.txt

foreach table ($table_a)

	hgsql -N -e "SELECT fileName, chrom, size from $table" $db > $db.$table.filename_chrom_size.txt
	gawk '{print $1 }' $db.$table.filename_chrom_size.txt > $db.$table.filename.txt

end

echo "============="
echo "Test if " $table_a " files are available"
echo 

# Test to see if file is accessible using the -e option

foreach file_path (`cat $db.$table_a.filename.txt`)

	if (-e $file_path) then
		echo "Found file " $file_path
	else
		echo "Can't find file " $file_path
	endif
echo
end

# Test to see if .nib file size is O.K. in chromInfo file

foreach file_path (`cat $db.$table_a.filename.txt`)

	# Use the nibSize Utility
	# Format: nib_path chrom_# size
	nibSize $file_path >> $db.$table_a.nibSize_output.txt
end

echo "diff the hgsql extract and nibSize extract"
echo "Should see no output if O.K."

diff $db.$table_a.nibSize_output.txt $db.$table.filename_chrom_size.txt
