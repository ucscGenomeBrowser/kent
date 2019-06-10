#!/usr/bin/env perl

use strict;
use warnings;

my %nameToChrom;	# key is seqName, value is chrom number
my %nameToAcc;	# key is seqName, value is accession
my %nameToUcsc;	# key is seqName, value is UCSC name

open (FH, "<sequence.name.Xref.tab") or die "can not read sequence.name.Xref.tab";
while (my $line = <FH>) {
  chomp $line;
  my ($seqName, $chrom, $accession) = split('\s+', $line);
  $nameToChrom{$seqName} = $chrom;
  $nameToAcc{$seqName} = $accession;
  my $noV = $accession;
  $noV =~ s/\./v/;
  my $ucscName;
  if ($seqName =~ m/MSCHRUN/) {
     $ucscName = "chrUN_$noV";
  } elsif (length($seqName) < 3) {
     $ucscName = "chr$seqName";
  } else {
     $ucscName = "chr${chrom}_${noV}_random";
  }  
  $nameToUcsc{$seqName} = $ucscName;
  printf "%s\t%s\t%d\n", $seqName, $ucscName, length($ucscName);
}
close (FH);

my $ucscChrFa = "start0.fa.gz";

open (CH, "|gzip -c > $ucscChrFa") or die "can not write to $ucscChrFa";

open (FH, "zcat ../sanger/mouse_GRCm38B_?.fa.gz|") or die "can not zcat ../sanger/mouse_GRCm38B_?.fa.gz";
while (my $line = <FH>) {
  if ($line =~ m/^>/) {
    close(CH);
    chomp $line;
    my $seqName = $line;
    $seqName =~ s/>chromosome:CURRENT://;
    $seqName =~ s/:.*//;
    my $ucscChrFa = "$nameToUcsc{$seqName}.fa.gz";
    open (CH, "|gzip -c > $ucscChrFa") or die "can not write to $ucscChrFa";
    printf CH ">%s\n", $nameToUcsc{$seqName};
    printf STDERR "writing to $ucscChrFa\n";
  } else {
    printf CH "%s", $line;
  }
}
close (FH);
close (CH);
