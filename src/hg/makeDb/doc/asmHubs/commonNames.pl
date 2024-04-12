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
if ($listFile eq "stdin") {
  open (FH, "</dev/stdin") or die "can not open /dev/stdin";
} else {
  open (FH, "<$listFile") or die "can not open $listFile";
}
while (my $asmId = <FH>) {
  next if ($asmId =~ m/^#/);
  $asmId =~ s/\s+.*//;
  chomp $asmId;
  next if (length($asmId) < 1);
  my @p = split('_', $asmId, 3);
  my $accession = "$p[0]_$p[1]";
  my $asmName = $p[2];
  my $asmNameAbbrev = $asmName;
  my $hapX = "";
  if ($asmNameAbbrev =~ m/hap1|hap2|alternate_haplotype|primary_haplotype/) {
     $hapX = $asmNameAbbrev;
     $asmNameAbbrev =~ s/.hap1//;
     $asmNameAbbrev =~ s/.hap2//;
     $asmNameAbbrev =~ s/_alternate_haplotype//;
     $asmNameAbbrev =~ s/_primary_haplotype//;
     $hapX =~ s/${asmNameAbbrev}.//;
     $asmNameAbbrev =~ s/_/ /g;
  }
  my $gcx = substr($asmId, 0, 3);
  my $id0 = substr($asmId, 4, 3);
  my $id1 = substr($asmId, 7, 3);
  my $id2 = substr($asmId, 10, 3);
  my $srcDir = sprintf "%s/%s/%s/%s/%s/%s", $ncbiSrc, $gcx, $id0, $id1, $id2, $asmId;
  my $asmRpt = "$srcDir/${asmId}_assembly_report.txt";
  if ( ! -s "${asmRpt}" ) {
    printf STDERR "%s\tmissing '%s'\n", $asmId, $asmRpt;
    next;
  }
  my $asmType = `grep -i -m 1 "Assembly type:" "${asmRpt}" | tr -d ""`;
  chomp $asmType;
  $asmType =~ s/.*bly type:\s+//i;
  if ($asmType eq "alternate-pseudohaplotype") {
    $asmType = " alternate hap";
  } elsif ($asmType =~ m/principal pseudo/) {
    $asmType = " primary hap";
  } else {
    $asmType = " $hapX";
  }
  my $sciName = `grep -i -m 1 "Organism name:" "${asmRpt}" | tr -d ""`;
  chomp $sciName;
  $sciName =~ s/.*ism name:\s+//i;
  $sciName =~ s/\s+\(.*\)$//;
  $sciName =~ s/\)//g;
  $sciName =~ s/\(//g;
  $sciName =~ s/\[//g;
  $sciName =~ s/\]//g;
  $sciName =~ s/\+//g;
  $sciName =~ s/\?/ /g;
  $sciName =~ s/\*//g;
  my $yearDate = `grep -i -m 1 "Date:" "${asmRpt}" | tr -d "" | awk '{print \$NF}' | sed -e 's/-.*//;'`;
  chomp $yearDate;
  my $isolate = `grep -i -m 1 "Isolate:" "${asmRpt}" | tr -d ""`;
  chomp $isolate;
  if (length($isolate)) {
    $isolate =~ s/.*solate: *//;
  }
  my $cultivar = `grep -i -m 1 "Infraspecific name:" "${asmRpt}" | tr -d ""`;
  chomp $cultivar;
  if (length($cultivar)) {
    $cultivar =~ s/.*cultivar=//;
    $cultivar =~ s/.*ecotype=//;
    $cultivar =~ s/.*strain=//;
    $cultivar =~ s/.*breed=//;
  }
  my $extraStrings = "";
  if (length($isolate) && length($cultivar)) {
     $extraStrings = "$cultivar ${isolate}$asmType $yearDate";
  } elsif (length($isolate)) {
     $extraStrings = "${isolate}$asmType $yearDate";
  } elsif (length($cultivar)) {
     $extraStrings = "${cultivar}$asmType $yearDate";
  }
  if ( length("${extraStrings}") < 1) {
     $extraStrings = "$asmType $yearDate";
     $extraStrings =~ s/^ +//;
  }
  my $orgName = `grep -i -m 1 "Organism name:" "${asmRpt}" | tr -d ""`;
  $orgName =~ s/.*\(//;
  $orgName =~ s/\)//g;
  $orgName =~ s/\(//g;
  $orgName =~ s/\[//g;
  $orgName =~ s/\]//g;
  $orgName =~ s/\?/ /g;
  $orgName =~ s/\+//g;
  $orgName =~ s/\*//g;
  chomp $orgName;
  if ($orgName =~ m/kinetoplastids|firmicutes|proteobacteria|high G|enterobacteria|agent of/) {
    $orgName = $sciName;
  } elsif ($orgName =~ m/bugs|crustaceans|nematodes|flatworm|ascomycete|basidiomycete|budding|microsporidian|smut|fungi|eukaryotes/) {
    my ($order, undef) = split('\s', $orgName, 2);
    $order = "budding yeast" if ($order =~ m/budding/);
    $order = "smut fungi" if ($order =~ m/smut/);
    $order = "ascomycetes" if ($order =~ m/ascomycete/);
    $order = "crustacean" if ($order =~ m/crustaceans/);
    $order = "flatworm" if ($order =~ m/flatworms/);
    $order = "nematode" if ($order =~ m/nematodes/);
    $order = "basidiomycetes" if ($order =~ m/basidiomycete/);
    my @a = split('\s+', $sciName);
    my $lastN = scalar(@a) - 1;
    if ($orgName =~ m/eukaryotes/) {
      $orgName = uc(substr($a[0], 0, 1)) . "." . "@a[1..$lastN]";
    } else {
      $orgName = "$order " . uc(substr($a[0], 0, 1)) . "." . "@a[1..$lastN]";
    }
  } elsif ($orgName eq "viruses") {
    $orgName = `grep -i -m 1 "Organism name:" "${asmRpt}" | tr -d ""`;
    chomp $orgName;
    $orgName =~ s/.*ism name:\s+//i;
    $orgName =~ s/\s+\(.*\)$//;
  }
  if (length($extraStrings)) {
    $extraStrings =~ s/\(//g;
    $extraStrings =~ s/\)//g;
    $extraStrings =~ s/\[//g;
    $extraStrings =~ s/\]//g;
    $extraStrings =~ s/\?/ /g;
    $extraStrings =~ s/\+//g;
    $extraStrings =~ s/\*//g;
    $extraStrings =~ s/${asmNameAbbrev} //;
    $extraStrings =~ s/  / /g;
    my @a = split('\s+', $extraStrings);
    for (my $i = 0; $i < scalar(@a); ++$i) {
        $orgName =~ s/$a[$i]//;
    }
    $orgName =~ s/=//g;
    $orgName =~ s/  / /g;
    $orgName =~ s/ +$//;
    $orgName =~ s/^ +//;
    if (length($orgName)) {
      printf "%s\t%s (%s)\n", $asmId, $orgName, $extraStrings;
    } else {
      printf "%s\t%s\n", $asmId, $extraStrings;
    }
  } else {
    printf "%s\t%s\n", $asmId, $orgName;
  }
}
close (FH);

# GCA_003369685.2_UOA_Angus_1_assembly_report.txt
# Organism name:

# GCF_010993605.1_kPetMar1.pri
# GCF_900246225.1_fAstCal1.2

