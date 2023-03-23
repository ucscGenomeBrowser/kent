#!/usr/bin/perl

# Parse a few columns out of Nextclade's voluminous output to help identify sequences
# that have only a subset of Omicron mutations so we can keep them from screwing up
# the base of the Omicron branch.

# Some bad sequences are assigned 19A, 20A, 20B but have a suspicious number of Omicron muts.
# Others are assigned Omicron (21K, 21L, 21M, 22*) but have a lot of reversions.

use warnings;
use strict;

my $maxOmicronMuts = 5;
my $maxReversions = 5;

# Column offsets:
#0       seqName
#1       clade
#18      privateNucMutations.reversionSubstitutions
#19      privateNucMutations.labeledSubstitutions
#30      nonACGTNs

# Examples values for a seq assigned to 21J (Delta) but with a suspicious number of Omicron muts
# (and Delta back-muts):
# reversions example: T4181G,T7124C,T8986C,T9053G,T16466C,G21618C,C27638T,T27752C,T29402G
# labeled example: T5386G|21K,G8393A|21K,C10449A|21K&21L&21M,A11537G|21K,T13195C|21K&21M,A17236G|21J,A18163G|21K&21L,C21762T|21K&21D&21M,C23525T|21K&20J&21L&21M,T23599G|21K&21L&21M,G23604A|20I&21K&21H&21L&21E&21M,G23948T|21K&21L,C24130A|21K&21M,C24503T|21K,A26530G|21K&21M,C26577G|21K&21L&21M,T27291C|21J,-28271T|21K&21G&21L&21M,C28311T|21K&21F&21G&21L&21M,T28881A|20I&21K&20B&20J&20F&20D&21G&21L&21E&21M

my $reversionsIx = 18;
my $labeledIx = 19;
my $ambigIx = 30;

sub cladeIsOmicron($) {
  my ($clade) = @_;
  return $clade =~ /^(21[KLM]|2[2-9]|recombinant)/;
}

sub reversionCount($$) {
  # Exclude ambiguous bases from reversions.  Aside from that:
  # Just return the number of mutations in the comma-sep list, no second-guessing, although
  # I've seen cases where a sequence is placed out at the end of a long branch and half of the
  # long branch muts are counted against it as reversions -- even though in the big tree, that
  # long branch is broken up many times and breaking it up would be usher's approach.  However,
  # in Nextclade's little tree, Omicron root is on a long branch, and in that case we do want
  # to count reversions against sequences that break up that particular long branch.
  # That's why I'm only looking at reversions (below) when the sequence is assigned to Omicron.
  my ($reversionStr, $ambigStr) = @_;
  $ambigStr =~ s/[A-Z]://g;
  my @revs = split(/,/, $reversionStr);
  my %ambigLocs = map {$_ => 1} split(/,/, $ambigStr);
  my $count = 0;
  foreach my $rev (@revs) {
    my $revLoc = $rev;
    $revLoc =~ s/^[A-Z](\d+)[A-Z]$/$1/;
    next if (exists $ambigLocs{$revLoc});
    $count++;
  }
  return $count;
}

sub offLabelCount($$) {
  my ($clade, $labeledStr) = @_;
  my $count = 0;
  my @mutStrs = split(',', $labeledStr);
  foreach my $mutStr (@mutStrs) {
    my (undef, $labelStr) = split(/\|/, $mutStr);
    if (index($labelStr, $clade) >= 0) {
      # The mutation is "private" by placement in nextclade's tree, but associated with this clade,
      # so I ignore it.  https://github.com/nextstrain/nextclade/issues/711
      next;
    }
    $count++;
  }
  return $count;
}

sub privateOmicronCount($$) {
  my ($clade, $labeledStr) = @_;
  my $count = 0;
  my @mutStrs = split(',', $labeledStr);
  foreach my $mutStr (@mutStrs) {
    my (undef, $labelStr) = split(/\|/, $mutStr);
    if (index($labelStr, $clade) >= 0) {
      # The mutation is "private" by placement in nextclade's tree, but associated with this clade,
      # so I ignore it.  https://github.com/nextstrain/nextclade/issues/711
      next;
    }
    if (index($labelStr, "21K") >= 0 || index($labelStr, "21L") >= 0 ||
        index($labelStr, "21M") >= 0) {
      $count++;
    }
  }
  return $count;
}

while (<>) {
  chomp;
  s/"//g;
  my @w = split("\t");
  my ($seqName, $clade, $reversionStr, $labeledStr, $ambigStr) =
    ($w[0], $w[1], $w[$reversionsIx], $w[$labeledIx], $w[$ambigIx]);
  if ($seqName eq 'seqName') {
    # Just in case they tweak the column order, if this looks like a header line, get the
    # indices from it:
    for (my $ix = 1;  $ix < @w;  $ix++) {
      if ($w[$ix] eq 'privateNucMutations.reversionSubstitutions') {
        $reversionsIx = $ix;
      } elsif ($w[$ix] eq 'privateNucMutations.labeledSubstitutions') {
        $labeledIx = $ix;
      } elsif ($w[$ix] eq 'nonACGTNs') {
        $ambigIx = $ix;
      }
    }
  } else {
    $clade =~ s/ .*//;
    my $isOmicron = cladeIsOmicron($clade);
    my $numReversions = reversionCount($reversionStr, $ambigStr);
    my $numPrivateOmicron = privateOmicronCount($clade, $labeledStr);
    if ((! $isOmicron && $numPrivateOmicron > $maxOmicronMuts) ||
        ($isOmicron && $numReversions > $maxReversions)) {
      print join("\t", $seqName, $clade, $numReversions, $numPrivateOmicron) . "\n";
    }
  }
}
