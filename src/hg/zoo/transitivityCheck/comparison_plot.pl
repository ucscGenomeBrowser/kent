#! /usr/bin/perl

# This script takes an "inferred" alignment (from the transitivity program) 
# and an unpacked original alignment and plots them.

# There are five arguments: inferred_align, actual_align, gif file name, 
#                           first sequence name, second sequence name

$inferred_name = $ARGV[0];
$actual_name = $ARGV[1];
$graphic_name = $ARGV[2];

$name_a=$ARGV[3];
$name_b=$ARGV[4];

$script = "temp.gnuplot";
$ppm= "temp.ppm";
 
# Create expanded files
`expand_coordinates.pl $inferred_name $inferred_name.expand`;
`expand_coordinates.pl $actual_name $actual_name.expand`;

open(OUT_FILE, ">$script") or die "Can't open output $script";

print OUT_FILE "set terminal pbm small color\n";
print OUT_FILE "set size 1.0 1.0\n";
print OUT_FILE "set title \"Comparison of $name_a and $name_b Alignments\"\n";
print OUT_FILE "show title\n";
print OUT_FILE "set xlabel \"$name_a\"\n";
print OUT_FILE "show xlabel\n";
print OUT_FILE "set ylabel \"$name_b\"\n";
print OUT_FILE "show ylabel\n";
print OUT_FILE "plot \"$inferred_name.expand\" title \"Inferred Alignment\", \"$actual_name.expand\" title \"Actual Alignment\"\n";
close OUT_FILE;

`gnuplot < $script > $ppm`;
`ppmtogif < $ppm > $graphic_name`;

`rm $ppm`;
 `rm $inferred_name.expand`;
 `rm $actual_name.expand`;    
 `rm $script`;






