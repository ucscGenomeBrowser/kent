#!/usr/bin/perl -w
use strict;
# File: createNcbiChrAgp
# Author: Terry Furey
# Date: 10/2001
# Description: Creates chromosome agp files.  Assumes that the
# lift files and contig agps have been created already 
# 2004-05-20 - Updated chr list, made strict safe - Hiram

# Usage message
if ($#ARGV < 0) {
  print STDERR "createNcbiChrAgp [-randomonly] <dir>\n";
  exit(1);
}

# Read in and check parameters
my $randomonly = 0;
while ($#ARGV > 0) {
  my $param = shift(@ARGV);
  if ($param eq "-randomonly") {
    $randomonly = 1;
  }
}
my $dir = shift(@ARGV);
if (! -e $dir) {
  die("$dir does not exist\n");
}

# Create chromosome agp files
#	foreach $chr (1..22,X,Y,qw(Un)) {
foreach my $chr (1..22,'X','Y','M','6_hla_hap1','6_hla_hap2') {
    if ((-e "$dir/$chr/lift/ordered.lft") && (!$randomonly)) {
	print STDERR "Creating chr$chr.agp\n";
	open(LFT, "<$dir/$chr/lift/ordered.lft") or
		die "Can not open $dir/$chr/lift/ordered.lft";
	open(OUT, ">$dir/$chr/chr$chr.agp") or
		die "Can not open $dir/$chr/chr$chr.agp";
	createAGP();
    }
    if (-e "$dir/$chr/lift/random.lft") {
	print STDERR "Creating chr${chr}_random.agp\n";
	open(LFT, "<$dir/$chr/lift/random.lft") or
		die "Can not open $dir/$chr/lift/random.lft";
	open(OUT, ">$dir/$chr/chr${chr}_random.agp") or
		die "Can not open $dir/$chr/chr${chr}_random.agp";
	createAGP();
    }
}
 
# Create actual file
sub createAGP
  {
 
    my $currStart = 1;
    my $prevend = 0;
    my $index = 0;
 
    # Go through all contigs in the chrom listed in lift file
    while (my $line = <LFT>) {
      chomp($line);
      (my $start, my $name, my $size, my $chrname, my $chrsize) =
	split(" ",$line);
      $start++;
      my $rand = 0;
      if ($chrname =~ /random/) {
        $rand = 1;
      }
      (my $chr,my $ctg) = split(/\//, $name);
 
      # Create a contig gap
      if ($start > ($prevend + 1)) {
        my $gapstart = $prevend + 1;
        my $gapend = $start - 1;
        my $gapsize = $gapend - $prevend;
        $index++;
        print OUT "$chrname\t$gapstart\t$gapend\t$index\tN\t$gapsize\tcontig\tno\n";
      }
 
      # Write out the contig agp with translated coordinates
      print STDERR "\tprocessing /${ctg}.agp\n";
      open(CTGAGP, "<$dir/$chr/$ctg/${ctg}.agp") or
	 die("Can not open $dir/$chr/$ctg/${ctg}.agp");
      while (my $agpline = <CTGAGP>) {
        chomp($agpline);
        my @agpline = split("\t", $agpline);
        $agpline[0] = $chrname;
        $agpline[1] = $start + $agpline[1] - 1;
        $agpline[2] = $start + $agpline[2] - 1;
        $index++;
        $agpline[3] = $index;
        $agpline = join("\t",@agpline);
        print OUT "$agpline\n";
      }
      $prevend = $start + $size - 1;
    }
  }

