#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1) {
  printf STDERR "usage: scanExonAA.pl NC_007793v1.exonAA.fa.gz > NC_007793v1.AA.profile.txt\n";
  exit 255;
} 

my %chromSizes;		# key is chr name, value is size

open (FH, "<../../../chrom.sizes") or die "can not read ../../../chrom.sizes";
while (my $line = <FH>) {
  chomp $line;
  my ($chr, $size) = split('\s+', $line);
  $chromSizes{$chr} = $size;
}
close (FH);

my %geneCdsStart;	# key is gene name, value is cdsStart

open (FH, "<geneName.cdsStart") or die "can not read geneName.cdsStart";
while (my $line = <FH>) {
  chomp $line;
  my ($name, $start) = split('\s+', $line);
  $geneCdsStart{$name} = $start;
}
close (FH);

# categories taken from:
# https://www.genscript.com/amino_acid_structure.html
# this index will translate the AA into an array index
my %aaIndex;
# Aliphatic Hydrophobic
$aaIndex{'A'} = 0;	# ALA Alanine
$aaIndex{'I'} = 1;	# ILE Isoleucine
$aaIndex{'L'} = 2;	# LEU Leucine
$aaIndex{'V'} = 3;	# VAL Valine
# Aromatic Hydrophobic
$aaIndex{'F'} = 4;	# PHE Phenylalanine
$aaIndex{'W'} = 5;	# TRP Tryptophan
$aaIndex{'Y'} = 6;	# TYR Tyrosine
# Neutral side chain
$aaIndex{'N'} = 7;	# ASN Asparagine
$aaIndex{'C'} = 8;	# CYS Cysteine
$aaIndex{'Q'} = 9;	# GLN Glutamine
$aaIndex{'M'} = 10;	# MET Methionine
$aaIndex{'S'} = 11;	# SER Serine
$aaIndex{'T'} = 12;	# THR Threonine
# Positive side chain
$aaIndex{'R'} = 13;	# ARG Arginine
$aaIndex{'H'} = 14;	# HIS Histidine
$aaIndex{'K'} = 15;	# LYS Lysine
# Negative side chain
$aaIndex{'D'} = 16;	# ASP Aspartic Acid
$aaIndex{'E'} = 17;	# GLU Glutamic Acid
# Unique
$aaIndex{'G'} = 18;	# GLY Glycine
$aaIndex{'P'} = 19;	# PRO Proline

my @aaName;
$aaName[0] = "ALA";
$aaName[1] = "ILE";
$aaName[2] = "LEU";
$aaName[3] = "VAL";
$aaName[4] = "PHE";
$aaName[5] = "TRP";
$aaName[6] = "TYR";
$aaName[7] = "ASN";
$aaName[8] = "CYS";
$aaName[9] = "GLN";
$aaName[10] = "MET";
$aaName[11] = "SER";
$aaName[12] = "THR";
$aaName[13] = "ARG";
$aaName[14] = "HIS";
$aaName[15] = "LYS";
$aaName[16] = "ASP";
$aaName[17] = "GLU";
$aaName[18] = "GLY";
$aaName[19] = "PRO";

my @aaSymbols;	# index is from aaIndex above, value is the symbol,
                # for the purpose of reverse lookup

my @outFiles;	# index is from aaIndex above, value is the output file handle

# setup the reverse lookup, array index to symbol
# and open the 20 output files
foreach my $aa (keys %aaIndex) {
  my $index = $aaIndex{$aa};
  $aaSymbols[$index] = $aa;
  my $outFileName = sprintf("%s.bed", $aaName[$index]);
  local *BD;
  open (BD, ">$outFileName") or die "can not write to $outFileName";
  $outFiles[$index] = *BD;
}

my $aaFile = shift;

if ($aaFile =~ m/.gz$/) {
  open (FH, "zcat $aaFile|") or die "can not zcat $aaFile";
} else {
  open (FH, "<$aaFile") or die "can not read $aaFile";
}

my $aaSequence = "";
my $geneName = "";
my $sequenceName = "";
my $chrName = "";
my $chrStrand = "";
my $cdsStart = 0;
my $aaLength = 0;
my $frame = 0;
my $lastFrame = 0;
my @counters = ();	# index is the AA position, value is an array pointer
                # to the 20 cell array for counting the AAs at this location
my $sequenceCounted = 0;

sub initCounters($) {
  my ($length) = @_;
  die "aaLength less than one ? $length" if ($length < 1);
  @counters = ();
  for (my $j = 0; $j < $length; ++$j) {
    my @aaCount;
    for (my $i = 0; $i < 20; ++$i) { $aaCount[$i] = 0; }
    $counters[$j] = \@aaCount;
  }
  $sequenceCounted = 0;
}

sub countAAs($$) {
  my ($length, $seq) = @_;
  for (my $j = 0; $j < $length; ++$j) {
    my $aaCount = $counters[$j];
    my $aa = uc(substr($seq,$j,1));
    next if ($aa eq "-");
    next if (($aa eq "Z") && ($j == $length-1));
#     if ($aa eq "Z") { printf STDERR "# Z found before end\n"; next;}
    next if ($aa eq "Z");
    next if ($aa eq "X");
#    if ($aa eq "X") { printf STDERR "# X found\n"; next;}
    die "aaIndex{$aa} not defined ?" if (! defined($aaIndex{$aa}));
    my $index = $aaIndex{$aa};
    $aaCount->[$index] += 1;
  }
  ++$sequenceCounted;
}

sub outputCounts($$) {
  my ($length,$name) = @_;
#  printf "%s\t%s\n", $name, $cdsStart;
  my $cdsEnd = $cdsStart + ($length * 3);
  for (my $i = 0; $i < 20; ++$i) {
#    printf "%s", $aaSymbols[$i];
    for (my $j = 0; $j < $length; ++$j) {
      my $aaCount = $counters[$j];
      if (defined($aaCount->[$i])) {
        if ($aaCount->[$i]) {
          my $start = $cdsStart + (3*$j);
#           my $end = $start + 3;
          if ($chrStrand eq "-") {
# printf STDERR "# negB: %s\t%d\t%d\t\t%d\t%s\t%s\n", $chrName, $start, $end, $aaCount->[$i], $chrStrand, $name;
             $start = $cdsEnd - ($start - $cdsStart) - 3;
#             $end = $start + 3;
# printf STDERR "# negA: %s\t%d\t%d\t\t%d\t%s\t%s\n", $chrName, $start, $end, $aaCount->[$i], $chrStrand, $name;
          }
          my $end = $start + 3;
          die "chrEnd $end GT $chromSizes{$chrName} chrSize for gene $geneName" if ($end > $chromSizes{$chrName});
          my $score = int(1000 * ($aaCount->[$i]/$sequenceCounted));
          my $colorIntensity = 255 * (int(10 * ($score / 1000)) / 10);
          my $backGround = 255 - $colorIntensity;
          $colorIntensity = 255;
          my $itemRgb = sprintf("%d,%d,%d", $backGround, $backGround, $colorIntensity);
          $itemRgb = sprintf("%d,%d,%d", $colorIntensity, $backGround, $backGround) if ($chrStrand eq "-");
          my $popUp = sprintf("%d%% (%d / %d)", int($score/10), $aaCount->[$i], $sequenceCounted);
          local *BD = $outFiles[$i];
          printf BD "%s\t%d\t%d\t\t%d\t%s\t%d\t%d\t%s\t%s\n", $chrName, $start, $end, $score, $chrStrand, $start, $end, $itemRgb, $popUp;
        }
#        printf " %d", $aaCount->[$i];
#      } else {
#        printf " 0";
      }
    }
#    printf "\n";
  }
}

my $prevLength = 0;
my $skipSequence = 0;

while (my $line = <FH>) {
  chomp $line;
  if ($line =~ m/^>/) {
    if (length($aaSequence) > 0) {
      if (length($aaSequence) != $prevLength) {
        printf STDERR "# error: length of sequence %d is not = expected %d\n", length($aaSequence), $aaLength;
        printf STDERR "# at line $. of $aaFile\n";
        die "check this out";
      }
      countAAs($prevLength, $aaSequence);
    }
    $skipSequence = 0;
    my @a = split('\s+', $line);
    $aaLength = $a[1];
    $frame = $a[2];
    $lastFrame = $a[3];
    $aaSequence = "";
    $sequenceName = $a[0];
    $geneName = $a[0];
    $geneName =~ s/^>//;
    if ($geneName =~ m/^[a-z]/) {
       $geneName=~ s/_.*//;
    } else {
       my @g = split('_', $geneName);
       $geneName = sprintf("%s_%s", $g[0], $g[1]);
    }
    $sequenceName =~ s/^>${geneName}_//;
    if ($sequenceName =~ m/Bacillus|E_coli/) {
      $skipSequence = 1;
#      printf STDERR "# skipping sequence: '$sequenceName' '$geneName'\n";
      next;
    }
#    printf STDERR "# working: $a[0] length $aaLength\n";
    if ($a[0] =~ m/staAur2/) {
      outputCounts($prevLength, $geneName) if ($sequenceCounted);

#      printf STDERR "# reference staAur2, initialize counters\n";
      initCounters($aaLength);
      $prevLength = $aaLength;
      die "can not find cdsStart for gene '$geneName'" if (!defined($geneCdsStart{$geneName}));
      $cdsStart = $geneCdsStart{$geneName};
      $chrName = $a[4];	# e.g.  NC_007793v1:544-1905+
      $chrName =~ s/:.*//;
      $chrStrand = substr($a[4],-1,1);	# last character
      printf STDERR "# %s %d %s %s %d\n", $chrName, $cdsStart, $chrStrand, $geneName, $aaLength;
    }
  } else {
    next if ($skipSequence);
    $aaSequence = sprintf("%s%s", $aaSequence, $line);
  }
}

close (FH);

outputCounts($prevLength, $geneName) if ($sequenceCounted);

foreach my $aa (keys %aaIndex) {
  my $index = $aaIndex{$aa};
  close($outFiles[$index]);
}

__END__

>SAUSA300_RS00010_staAur2_1_1 454 0 0 NC_007793v1:544-1905+
MSEKEIWEKVLEIAQEKLSAVSYSTFLKDTELYTIKDGEAIVLSSIPFNANWLNQQYAEIIQAILFDVVGYEVKPHFITTEELANYSNNETATPKETTKPSTETTEDNHVLGREQFNAHNTFDTFVIGPGNRFPHAASLAVAEAPAKAYNPLFIYGGVGLGKTHLMHAIGHHVLDNNPDAKVIYTSSEKFTNEFIKSIRDNEGEAFRERYRNIDVLLIDDIQFIQNKVQTQEEFFYTFNELHQNNKQIVISSDRPPKEIAQLEDRLRSRFEWGLIVDITPPDYETRMAILQKKIEEEKLDIPPEALNYIANQIQSNIRELEGALTRLLAYSQLLGKPITTELTAEALKDIIQAPKSKKITIQDIQKIVGQYYNVRIEDFSAKKRTKSIAYPRQIAMYLSRELTDFSLPKIGEEFGGRDHTTVIHAHEKISKDLKEDPIFKQEVENLEKEIRNVZ
>SAUSA300_RS00010_Sa_USA300_SUR12_1_1 454 0 0 NZ_CP014407v1:544-1905+
MSEKEIWEKVLEIAQEKLSAVSYSTFLKDTELYTIKDGEAIVLSSIPFNANWLNQQYAEIIQAILFDVVGYEVKPHFITTEELANYSNNETATPKETTKPSTETTEDNHVLGREQFNAHNTFDTFVIGPGNRFPHAASLAVAEAPAKAYNPLFIYGGVGLGKTHLMHAIGHHVLDNNPDAKVIYTSSEKFTNEFIKSIRDNEGEAFRERYRNIDVLLIDDIQFIQNKVQTQEEFFYTFNELHQNNKQIVISSDRPPKEIAQLEDRLRSRFEWGLIVDITPPDYETRMAILQKKIEEEKLDIPPEALNYIANQIQSNIRELEGALTRLLAYSQLLGKPITTELTAEALKDIIQAPKSKKITIQDIQKIVGQYYNVRIEDFSAKKRTKSIAYPRQIAMYLSRELTDFSLPKIGEEFGGRDHTTVIHAHEKISKDLKEDPIFKQEVENLEKEIRNVZ
