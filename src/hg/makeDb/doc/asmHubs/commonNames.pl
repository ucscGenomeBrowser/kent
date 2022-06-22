#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1){
  printf STDERR "usage: ./commonNames.pl vgp.2020-04-24.list\n";
  printf STDERR "will look up the common names from the assembly_report files\n";
  exit 255;
}

my $ncbiSrc="/hive/data/outside/ncbi/genomes";

my $listFile = shift;
open (FH, "<$listFile") or die "can not open $listFile";
while (my $asmId = <FH>) {
  next if ($asmId =~ m/^#/);
  $asmId =~ s/\s+.*//;
  chomp $asmId;
  next if (length($asmId) < 1);
  my $gcx = substr($asmId, 0, 3);
  my $id0 = substr($asmId, 4, 3);
  my $id1 = substr($asmId, 7, 3);
  my $id2 = substr($asmId, 10, 3);
  my $srcDir = sprintf "%s/%s/%s/%s/%s/%s", $ncbiSrc, $gcx, $id0, $id1, $id2, $asmId;
  my $asmRpt = "$srcDir/${asmId}_assembly_report.txt";
  my $orgName = `grep -i -m 1 "Organism name:" "${asmRpt}" | tr -d ""`;
  $orgName =~ s/.*\(//;
  $orgName =~ s/\)//;
  chomp $orgName;
  printf "%s\t%s\n", $asmId, $orgName;
}
close (FH);

# GCA_003369685.2_UOA_Angus_1_assembly_report.txt
# Organism name:

# GCF_010993605.1_kPetMar1.pri
# GCF_900246225.1_fAstCal1.2

