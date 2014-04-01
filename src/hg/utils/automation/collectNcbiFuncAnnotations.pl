#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/collectNcbiFuncAnnotations.pl instead.

use warnings;
use strict;

# Gather up multiple annotation lines into one line per {snp, gene, frame}:
my ($lastRs, $lastCtg, $lastS, $lastE, $lastTx, $lastFrm);
my ($count, $fxns, $nts, $codons, $aas, $refRow);
while (<>) {
  chomp;
  my ($rsId, $ctg, $s, $e, $txId, $fxn, $frm, $nt, $aa, $codon) = split("\t");
  # Ignore cds-reference with no other info:
  next if ($fxn == 8 && ($frm eq "NULL" && $aa eq "NULL" && $codon eq "NULL"));
  if (defined $lastRs &&
      ($lastRs != $rsId || $lastCtg ne $ctg || $lastS != $s ||
       $lastTx ne $txId || $lastFrm ne $frm)) {
    if (defined $refRow) {
      if (! defined $fxns) {
	($fxns, $nts, $aas, $codons) = ("", "", "", "");
      }
      $fxns = "$refRow->[0],$fxns";  $nts = "$refRow->[1],$nts";
      $aas = "$refRow->[2],$aas";    $codons = "$refRow->[3],$codons";
    }
    my $lineOut = join("\t", $lastCtg, $lastS, $lastE, "rs$lastRs", $lastTx, $lastFrm,
		       $count, $fxns, $nts, $codons, $aas) . "\n";
    $lineOut =~ s@NULL@n/a@g;
    print $lineOut;
    $refRow = undef;  ($count, $fxns, $nts, $codons, $aas) = ();
  }
  ($lastRs, $lastCtg, $lastS, $lastE, $lastTx, $lastFrm) =
    ($rsId, $ctg, $s, $e, $txId, $frm);
  $count++;
  if ($fxn == 8) {
    $refRow = [$fxn, $nt, $aa, $codon];
  } else {
    $fxns .= "$fxn,";  $nts .= "$nt,";  $aas .= "$aa,";  $codons .= "$codon,";
  }
}
if (defined $refRow) {
  $fxns = "$refRow->[0],$fxns";  $nts = "$refRow->[1],$nts";
  $aas = "$refRow->[2],$aas";    $codons = "$refRow->[3],$codons";
}
my $lineOut = join("\t", $lastCtg, $lastS, $lastE, "rs$lastRs", $lastTx, $lastFrm,
		   $count, $fxns, $nts, $codons, $aas) . "\n";
$lineOut =~ s@NULL@n/a@g;
print $lineOut;

