#! /usr/bin/perl

# This script inverts the coordinates of a given file
# (e.g. from 1 2 3 4 to 3 4 1 2) and outputs the result
# in a new file

$in_name = $ARGV[0];
$out_name = $ARGV[1];

#Open old file and new file    

open(IN_FILE, $in_name) or die "Can't open input $in_name";  
open(OUT_FILE, ">$out_name") or die "Can't open output $out_name";    

# Invert Coordinates, print to new file

while($line = <IN_FILE>)
{	
    $line =~ s/(\d+)\s+(\d+)\s+(\d+)\s+(\d+)/$3 $4 $1 $2/g;
    print OUT_FILE $line;
}





