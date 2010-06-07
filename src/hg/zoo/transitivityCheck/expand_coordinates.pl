#! /usr/bin/perl

# This script expands the coordinates of a given file
# So that 1 4 10 14 becomes
# 1 10
# 2 11
# 3 12
# 4 13
# The first file is the file which has the source coordinates
# The second file is the file that contains the expanded 
# Coordinates.

$in_name = $ARGV[0];
$out_name = $ARGV[1];

#Open old file and new file    

open(IN_FILE, $in_name) or die "Can't open input $in_name";  
open(OUT_FILE, ">$out_name") or die "Can't open output $out_name";    

# Invert Coordinates, print to new file

while($line = <IN_FILE>)
{	
    $line =~ /(\d+)\s+(\d+)\s+(\d+)\s+(\d+)/;
    $counter = $1;
    $counter_2 = $3;
    
    while($counter <= $2)
    {
	print OUT_FILE "$counter $counter_2\n";
	$counter++;
	$counter_2++;
    }
}





