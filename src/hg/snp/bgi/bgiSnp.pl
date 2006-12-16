#!/usr/bin/perl -w
# Digest BGI files into bgiSnp bed+.

use Carp;
use strict;

my $snpOut = 'bgiSnp.bed';

sub parseFileName {
  # Get strain and chrom from filename.
  my $fileName = shift;
  my ($strain, $chr);
  if ($fileName =~ m/(Broiler|Layer|Silkie)/) {
    $strain = $1;
  } else {
    die "Can't get strain [Broiler,Layer,Silkie] from filename ($fileName)";
  }
  if ($fileName =~ m/(chr\w[\w_]*)\b/) {
    $chr = $1;
  } else {
    die "Can't get chrom from filename ($fileName)";
  }
  return ($strain, $chr);
}

#BEGIN Copied from bgiXref.pl:
# SNP position is either a single 1-based coord or 1-based start,end
sub parsePos {
  my $pos = shift;
  my ($start, $end);
  if ($pos =~ /^(\d+),(\d+)$/) {
    $start = $1 - 1;
    $end = $2;
  } elsif ($pos =~ /^\d+$/) {
    $start = $pos - 1;
    $end = $pos;
  } else {
    confess "Bad pos $pos";
  }
  return($start, $end);
}

sub parseStrains {
  my $strains = shift;
  my ($inB, $inL, $inS);
  if ($strains =~ /^([B\-X])([L\-X])([S\-X])$/) {
    $inB = &parseStrain($1);
    $inL = &parseStrain($2);
    $inS = &parseStrain($3);
  } else {
    confess "Bad strains $strains";
  }
  return($inB, $inL, $inS);
}
sub parseStrain {
  my $strain = shift;
  if ($strain eq '-') {
    return('no');
  } elsif ($strain eq 'X') {
    return('n/a');
  } else {
    return('yes');
  }
}

#END Copied from bgiXref.pl

#######################################################################
# MAIN

my %snpIds = ();
my $alreadyWarnedStrain = 0;
open(SNP, ">$snpOut") || die "Can't open >$snpOut: $!\n";
foreach my $fileName (@ARGV) {
  my ($strain, $chr) = &parseFileName($fileName);
  open(IN, "$fileName") || die "Can't open $fileName: $!\n";
  while (<IN>) {
    chomp;
    my ($snpId, $type, $pos, $rPos, $qualChr, $qualReads, $snpSeq, undef,undef,
	$readName, $readDir, $strains, $primerL, $primerR, undef, undef, $conf,
	$extra) = split;
    my ($chromStart, $chromEnd) = &parsePos($pos);
    my ($readStart, $readEnd) = &parsePos($rPos);
    my ($inB, $inL, $inS) = &parseStrains($strains);
    $readDir = ($readDir eq 'F') ? '+' : '-';
    $extra = "" if (! defined $extra);
    my $expectedStrainCode;
    if ($strain eq 'Broiler') {
      $expectedStrainCode = 1;
    } elsif ($strain eq 'Layer') {
      $expectedStrainCode = 2;
    } elsif ($strain eq 'Silkie') {
      $expectedStrainCode = 3;
    } else {
      die "Error: strain is $strain and it must be Broiler, Layer or Silkie.";
    }
    if ($snpId =~ m/(\d)$/ && $1 != $expectedStrainCode) {
      if (!$alreadyWarnedStrain) {
	warn "$snpId has strain code (last digit) that doesn't jive with " .
	  "strain ($strain) [Broiler=1, Layer=2, Silkie=3]; tweaking it to " .
	  $expectedStrainCode . "\n" .
	  "There may be a lot of these... check input and ping BGI.\n";
	$alreadyWarnedStrain = 1;
      }
      $snpId =~ s/\d$/$expectedStrainCode/;
    }
    if (defined $snpIds{$snpId}) {
      die "Duplicate def for $snpId, line $. of $fileName:\n$_\n";
    }
    $snpIds{$snpId} = 1;
    print SNP "$chr\t$chromStart\t$chromEnd\t$snpId\t" .
      "$type\t$readStart\t$readEnd\t$qualChr\t$qualReads\t$snpSeq\t" .
      "$readName\t$readDir\t$inB\t$inL\t$inS\t$primerL\t$primerR\t$conf\t" .
      "$extra\n";
  }
  close(IN);
}
close(SNP);

