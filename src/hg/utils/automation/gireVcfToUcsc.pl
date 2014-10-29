#!/usr/bin/env perl
# Convert supplemental VCF from http://www.sciencemag.org/content/345/6202/1369/suppl/DC1
#   "Genomic surveillance elucidates Ebola virus origin and transmission during the 2014 outbreak"
#   Gire et al, Science 12 September 2014: vol. 345 no. 6202 pp. 1369-1372
# from being KM034562-referenced to KJ660347-referenced so we can display in eboVir2.
# For the 7 SNVs between KJ660347 and KM034562, swap REF and ALT alleles.

# TODO: unfortunately the used KJ66034{6,7,8} .1 not .2 like we have... so there are several
# more diffs that should be corrected in those, ugh.

use warnings;
use strict;

my @diffs = ();
$diffs[1849] = ['T', 'C'];
$diffs[6283] = ['C', 'T'];
$diffs[9923] = ['C', 'T'];
$diffs[11811] = ['C', 'T'];
$diffs[13856] = ['A', 'G'];
$diffs[15599] = ['A', 'G'];
$diffs[15660] = ['T', 'C'];

while (<>) {
    if (/^#/) {
      print;
      next;
    }
    chomp;
    my @w = split("\t");
    $w[0] = "KJ660347v2";
    my $pos = $w[1];
    if ($diffs[$pos]) {
      my ($ref, $alt) = @{$diffs[$pos]};
      my ($gRef, $gAlt) = ($w[3], $w[4]);
      if ($gRef eq $alt && $gAlt eq $ref) {
        # Swap REF and ALT, and invert all of the 0/1 genotypes (if any):
        ($w[3], $w[4]) = ($ref, $alt);
        for (my $i = 9;  $i <= $#w;   $i++) {
          $w[$i] =~ /^([01.])(:.*)?/ || die "Can't match $w[$i]";
          my ($gt, $rest) = ($1, $2);
          $gt = 1 - $gt if ($gt ne ".");
          $gt .= $rest if ($rest);
          $w[$i] = $gt;
        }
      } elsif ($gRef ne $ref || $gAlt ne $alt) {
        # If this ever happens we'll have to get fancier (make a tri-allelic record):
        die "tri-allelic? KM $gRef/$gAlt vs KJ $ref/$alt\n\t";
      }
    }
    print join("\t", @w) . "\n";
}
