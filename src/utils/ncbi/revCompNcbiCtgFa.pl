#!/usr/bin/perl
# File: revCompNcbiCtgFa
# Author: Terry Furey
# Date: 10/2001
# Description: Reverse complement NT_XXXX.fa that need to be
# based on the NCBI build

# Usage message
if ($#ARGV != 1) {
  print stderr "revCompNcbiCtgFa <seq_contig.md> <dir>\n";
  exit(1);
}

# Read in and check arguments
$file = shift(@ARGV);
$dir = shift(@ARGV);
open(FILE, "<$file") || die("Could not open $file\n");
if (! -e "$dir") {
  die("$dir does not exist\n");
}

# Process each line in the seq_contig.md file
# which lists the contigs and orientations in the build
while ($line = <FILE>) {
  chomp($line);
  ($build, $thischr, $start, $end, $orien, $thisctg, $id, $type, $weight) 
= split("\t",$line);

  # Skip non-contig related lines and lines
  # where the orientation is correct (+) or unknown (?)
  next if ($thisctg eq "start"); # changed 'ctg' to 'thisctg' on 3/29/2002 DJT
  next if ($thisctg eq "end");   # changed 'ctg' to 'thisctg' on 3/29/2002 DJT
  next if ($orien ne "-");

  ($ctg, $vers) = split(/\./, $thisctg);
  ($chr, $other) = split(/\|/, $thischr);

  # Check that contig exists, reverse complement it	 
  if (! -e "$dir/$chr/$ctg/$ctg.fa") {
    print stderr ("$dir/$chr/$ctg/$ctg.fa does not exist\n");
  }
  print stderr "complementing  $dir/$chr/$ctg/$ctg.fa\n";
  print `mv $dir/$chr/$ctg/$ctg.fa $dir/$chr/$ctg/$ctg.fa.orig`;
  print `/cluster/bin/i386/faRc  -keepName $dir/$chr/$ctg/$ctg.fa.orig $dir/$chr/$ctg/$ctg.fa`;
}
