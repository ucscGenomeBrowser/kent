#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1 && $ARGV[0] ne "makeItSo" ) {
  printf STDERR "usage: ./genArkListToSql.pl makeItSo > genark.tsv\n";
  printf STDERR "used by updateGenArkCentral.sh to translate the\n";
  printf STDERR "assembly list to tsv for loading into SQL table.\n";
  exit 255;
}

open (FH, "grep -v '^#' UCSC_GI.assemblyHubList.txt|") or die "can not grep UCSC_GI.assemblyHubList.txt";
while (my $line = <FH>) {
  chomp $line;
  my ($accession, $assembly, $sciName, $commonName, $taxId)=split('\t', $line);
  my $gcX = substr($accession, 0, 3);
  my $d0 = substr($accession, 4, 3);
  my $d1 = substr($accession, 7, 3);
  my $d2 = substr($accession, 10, 3);
  my $path="$gcX/$d0/$d1/$d2";
  my $hubTxt = "$path/$accession/hub.txt";
  printf "%s\t%s\t%s\t%s\t%s\t%s\n", $accession, $hubTxt, $assembly, $sciName, $commonName, $taxId;
}
close (FH);

__END__

# (some assemblies have taxonId at this time, soon to have them all)
#
# accession     assembly        scientific name common name     taxonId
GCA_003369685.2 UOA_Angus_1     Bos indicus x Bos taurus        hybrid cattle   30522
GCA_003957525.1 bTaeGut1_v1.h   Taeniopygia guttata     zebra finch     59729
GCA_003957555.2 bCalAnn1_v1.p   Calypte anna    Anna's hummingbird      9244
