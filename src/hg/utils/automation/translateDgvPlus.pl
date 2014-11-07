#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/translateDgvPlus.pl instead.

use warnings;
use strict;

# DGV color coding:
my $purple = "200,0,200";  # Inversion
my $red = "200,0,0";       # Deletion/Loss
my $blue = "0,0,200";      # Insertion/Duplication/Gain
my $brown = "139,69,19";   # CNV (gain/loss unspecified)

my %varColors = ( 'CNV' => $brown,
		  'Complex' => $brown,
		  'Deletion' => $red,
		  'Duplication' => $blue,
		  'Gain' => $blue,
		  'Gain+Loss' => $brown,
		  'Insertion' => $blue,
		  'Inversion' => $purple,
		  'Loss' => $red,
		  'Tandem duplication' => $blue,
                  'complex' => $brown,
                  'deletion' => $red,
                  'duplication' => $blue,
                  'gain' => $blue,
                  'gain+loss' => $brown,
                  'insertion' => $blue,
                  'inversion' => $purple,
                  'loss' => $red,
                  'mobile element insertion' => $blue,
                  'novel sequence insertion' => $blue,
                  'sequence alteration' => $brown,
                  'tandem duplication' => $blue,
		);

while (<>) {
  next if (/^#/ || /^variantaccession/);
  chomp;
  my ($id, $chr, $s, $e, $varType, $varSubType, $ref, $pubMedId, $method, $platform,
     $mergedVars, $supVars, $mergedOrSamp, $freq, $sampleSize, $obsGains, $obsLosses,
     $sampleDesc, $genes, $sampleIds) = split("\t");
  # We don't use these columns, avoid 'unused variable' warning here:
  ($varType, $mergedOrSamp, $freq) = ();
  $chr = "chr$chr" unless ($chr =~ /^chr/);
  if ($s == 0) {
    warn "Got start=0, strange...";
  } else {
    $s--;
  }
  my $color = $varColors{$varSubType};
  if (! defined $color) {
    warn "Don't know what color to use for variantsubtype='$varSubType'";
    $color = "0,0,0";
  }
  $ref =~ s/_/ /g;
  $method =~ s/_/ /g;  $method =~ s/,/, /g;
  $supVars =~ s/""//;
  $genes =~ s/,/, /g;
  $obsGains = 0 if ($obsGains eq '');
  $obsLosses = 0 if ($obsLosses eq '');
  $genes =~ s/""//;

  print join("\t", $chr, $s, $e, $id, 0, '+', $s, $s, $color, $varSubType, $ref, $pubMedId,
	     $method, $platform, $mergedVars, $supVars, $sampleSize, $obsGains, $obsLosses,
	     $sampleDesc, $genes, $sampleIds) . "\n";
}


