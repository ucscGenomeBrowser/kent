#!/usr/bin/perl
# File: createNcbiLifts
# Author: Terry Furey
# Date: 10/2001
# Description: Creates lift files for an NCBI assembly.  Will
# create the chromosome directory structure if it doesn't exist
# and inserts file showing locations of centromeres and heterochromatin
 
# Usage message
if ($#ARGV < 1) {
  print stderr "createNcbiLifts [-s chrom_sizes] <seq_contig.md> <out dir>\n";
  exit(1);
}

# Read and check arguments
$sizes = "";
while ($#ARGV > 1) {
  $arg = shift(@ARGV);
  if ($arg eq "-s") {
    $sizes = shift(@ARGV);
    open(SIZES, "$sizes") || die("Could not open $sizes\n");
  }
}
$in = shift(@ARGV);
$dir = shift(@ARGV);
open(FILE, "<$in") || die("Could not open $in\n");
if (!-e $dir) {
  die("$dir does not exist\n");
}
 
# Determine lengths of chromosomes
$currchr = "";
$prevfinish = 0;
while ($line = <FILE>) {
  chomp($line);
  ($build, $chr, $start, $finish, $orien, $ctg, $id, $type, $weight) = split("\t",$line);
  # Record last end base position for each chromosome
  if ($chr ne $currchr) {
    $thischr = "chr$currchr";
    $total{$thischr} = $prevfinish;
    $currchr = $chr;
  }
  # Check for random contigs - space by 50000
  if ($chr =~ /\|/) {
    ($chr,$rest) = split(/\|/,$chr);
    $thischr = "chr${chr}_random";
    $total{$thischr} += $finish + 50000;
  }
  $prevfinish = $finish;
}
close(FILE);

# Revise lengths if there is a sizes file
if ($sizes) {
  while ($line = <SIZES>) {
    chomp($line);
    ($chr, $size) = split("\t",$line);
    $total{$chr} = $size;
  }
  close(SIZES);
}


# Create inserts file and header
open(INSERTS, ">inserts") || die("Could not create inserts file\n");
print INSERTS "#Chrom\tBefore_Contig\tAfter_Contig\tNum_bases\tType\n";

# Create lift files
$currChr = "";
$prevrandom = 0;
$prevctg = "-";
$prevend = 0;
open(FILE, "<$in") || die("Could not open $in\n");

while ($line = <FILE>) {
  chomp($line);
  ($build, $chr, $start, $finish, $orien, $thisctg, $id, $type, $weight) = 
      split("\t",$line);
  $start -= 1;
  next if ($thisctg eq "start");
  next if ($thisctg eq "end");
 
  ($ctg,$vers) = split(/\./,$thisctg);
 
  # Check if random contig
  $random = 0;
  if ($chr =~ /\|/) {
    ($chr,$rest) = split(/\|/,$chr);
    $random = 1;
  }
  # Create directories and open files
  if (($chr ne $currChr) || ($random ne $prevrandom)) {
    if (!-e "$dir/$chr") {
      `mkdir $dir/$chr`;
    }
    if (!-e "$dir/$chr/lift") {
      `mkdir $dir/$chr/lift`;
    }
    $prevend = 0;
    $prevctg = "-";
    if ($random) {
      print stderr "creating $chr/lift/random\n";
      open(LFT, ">$dir/$chr/lift/random.lft");
      open(LST, ">$dir/$chr/lift/random.lst");
      open(RMASK, ">$dir/$chr/lift/rOut.lst");
      open(AGP, ">$dir/$chr/lift/rAgp.lst");
      $thischr =  "chr${chr}_random";
      $total{$thischr} -= 50000;
    } else {
      print stderr "creating $chr/lift/ordered\n";
      open(LFT, ">$dir/$chr/lift/ordered.lft");
      open(LST, ">$dir/$chr/lift/ordered.lst");
      open(RMASK, ">$dir/$chr/lift/oOut.lst");
      open(AGP, ">$dir/$chr/lift/oAgp.lst");
    }
  }

  # Print name of contig to *.lst files
  print LST "$ctg\n";
  print RMASK "$ctg/$ctg.fa.out\n";
  print AGP "$ctg/$ctg.agp\n";

  # Check if it is an large gap insert
  $gap = $start - $prevend;
  if ($gap > 1000000) {
      print INSERTS "chr$chr\t$prevctg\t$ctg\t$gap\t";
      if ($prevctg eq "-") {
  	  print INSERTS "short_arm\n";
      } elsif ($gap > 3100000) {
  	  print INSERTS "heterochromatin\n";
      } else {
  	  print INSERTS "centromere\n";
      }
  }

  # Calculate length of contig and print to .lft file
  $length = $finish - $start;
  if ($random) {
    $thischr = "chr${chr}_random";
    if ($prevend > 0) {
	$start = $prevend + 50000;
    } else {
	$start = 0;
    }
  } else {
    $thischr = "chr$chr";
  }
  print stderr "Total for $thischr is $total{$thischr}\n";
  print LFT "$start\t$chr/$ctg\t$length\t$thischr\t$total{$thischr}\n";
  # print LFT "$start\t$chr/RC_$ctg\t$length\t$thischr\t$total{$thischr}\n";
  $currChr = $chr;
  $prevrandom = $random;
  $prevend = $start + $length;
  $prevctg = $ctg;
}



