#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;

my $argc = scalar(@ARGV);
if ($argc < 1) {
  printf STDERR "usage: ./idFromAsmRpt.pl <some.assembly_report.txt> [more reports ...]\n";
  exit 255
}

my $expectCount = 8;

while (my $asmRpt = shift) {
  open (FH, "<$asmRpt") or die "can not read $asmRpt";
  my $asmName="asmName not found";
  my $orgName="orgName not found";
  my $taxId="taxId not found";
  my $submitter="submitter not found";
  my $date="date not found";
  my $asmType="asmType not found";
  my $asmLevel="asmLevel not found";
  my $gbAsmId="gbAsmId not found";
  my $gotEnough = 0;
  while (($gotEnough < $expectCount) && (my $line = <FH>)) {
    chomp $line;
    next if ($line !~ m/^#/);
    if ($line =~ m/#\s+Assembly Name:/) {
      $line =~ s/.*Assembly Name:\s+//;
      $line =~ s/\s+$//;
      $asmName = $line;
      ++$gotEnough;
    } elsif ($line =~ m/Organism name:/) {
      $line =~ s/.*Organism name:\s+//;
      $line =~ s/\s+$//;
      $orgName = $line;
      $orgName =~ s/\(//g;
      $orgName =~ s/\)//g;
      ++$gotEnough;
    } elsif ($line =~ m/Taxid:/) {
      $line =~ s/.*Taxid:\s+//;
      $line =~ s/\s+$//;
      $taxId = $line;
      ++$gotEnough;
    } elsif ($line =~ m/Submitter:/) {
      $line =~ s/.*Submitter:\s+//;
      $line =~ s/\s+$//;
      $submitter = $line;
      ++$gotEnough;
    } elsif ($line =~ m/Date:/) {
      $line =~ s/.*Date:\s+//;
      $line =~ s/\s+$//;
      my ($Y,$m,$d) = split('-', $line);
      $date = sprintf("%4d-%02d-%02d", $Y, $m, $d);
      ++$gotEnough;
    } elsif ($line =~ m/Assembly type:/) {
      $line =~ s/.*Assembly type:\s+//;
      $line =~ s/\s+$//;
      $asmType = $line;
      ++$gotEnough;
    } elsif ($line =~ m/Assembly level:/) {
      $line =~ s/.*Assembly level:\s+//;
      $line =~ s/\s+$//;
      $asmLevel = $line;
      ++$gotEnough;
    } elsif ($line =~ m/GenBank Assembly ID:/) {
      if ($gbAsmId eq "gbAsmId not found") {  # take only the first one
        $line =~ s/.*GenBank Assembly ID:\s+//;
        $line =~ s/\s+.*$//;
        $gbAsmId = $line;
        ++$gotEnough;
      }
    } elsif ($line =~ m/GenBank Assembly Accession:/) {
      if ($gbAsmId eq "gbAsmId not found") {  # take only the first one
        $line =~ s/.*GenBank Assembly Accession:\s+//;
        $line =~ s/\s+.*$//;
        $gbAsmId = $line;
        ++$gotEnough;
      }
    }
  }
  close (FH);
  printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", $date, $taxId, $asmName, $orgName, $submitter, $asmType, $asmLevel, $gbAsmId;
  my $asmDir = dirname($asmRpt);
  $asmDir =~ s#^\./##;
  printf STDERR "%s\t%s\n", $asmName, $asmDir; 
}


# Assembly Name:  Vber.be_1.0
# Organism name:  Vipera berus berus
# Taxid:          31156
# Submitter:      BCM-HGSC
# Date:           2014-12-10

# Assembly type:  haploid
# Release type:   major
# Assembly level: Scaffold

