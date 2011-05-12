#!/usr/bin/perl

use warnings; use strict;



#grab list of composites from file

my $list = $ARGV[0];

open IN, "$list";

my @list;

while (<IN>){
	my $line = $_;
	chomp $line;
	push (@list, $line);

}

#main loop here

my $err;

foreach my $composite (@list){

	$composite =~ s/\s//;
	#print "$composite\n";
	my $file = "$composite.ra";
	print "file = $file\n";
	#sleep 1000;

	$err = `mdbPrint hg18 -composite=$composite > $file`;
	$err = `mdbUpdate hg18 -composite=$composite -encodeExp=encodeExp`;
	print STDOUT "cmd = mdbUpdate hg18 -composite=$composite -encodeExp=encodeExp\n err = $err\n";
	print "here2\n";
	$err = `mdbPrint hg18 -composite=$composite > $file`;
	print STDOUT "cmd = mdbPrint hg18 -composite=$composite > $file\n err = $err\n";
	print STDOUT "here3\n";	
	#sleep 100;

	open COMP, "$file";
	my @lines;

	while (<COMP>) {
		my $line = $_;
		chomp $line;
		push (@lines, $line); 
	}
	close COMP;
	my @newlines;

	foreach my $line (@lines){
		if ($line =~ m/dccAccession/){
			print STDOUT "removed a dccAccession\n";
		}
		else {
			push (@newlines, $line);
		}
	}
	open COMP, ">$file";
	foreach my $line (@newlines){
		print COMP "$line\n";
	}	
	close COMP;

	$err = `mdbUpdate hg18 $file -replace`;
	print STDOUT "cmd = mdbUpdate hg18 $file -replace\n err = $err\n";
	#sleep 100;
	$err = `mdbPrint hg18 -composite=$composite > $file`;

	print STDOUT "cmd = mdbPrint hg18 -composite=$composite > $file\n err = $err\n";


	print STDOUT "done with $composite\n";
	
	#sleep 10;







}














