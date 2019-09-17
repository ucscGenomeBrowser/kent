#!/usr/bin/env perl

# Convert NCBI assembly alt_scaffold_placement.txt to BED4 that relates alt and fix sequences
# to chromosome locations (or in one case as of 6/20/18, alt to fix).
# Optionally use *_assembly_report.txt file to determine alt vs. fix status -- default to guessing
# based on whether the Assembly name ends in "_PATCH".  (That doesn't work for alt HG1133_PATCH!)

use Getopt::Long;
use warnings;
use strict;

use vars qw/$opt_db $opt_assemblyReport/;

my $ok = GetOptions('db=s',
                    'assemblyReport=s');

my %aNameToSuffix = ();
if ($opt_assemblyReport) {
  open(my $F, $opt_assemblyReport) || die "Can't open $opt_assemblyReport: $!\n";
  while (<$F>) {
    next if (/^#/);
    my ($name, $type) = split("\t");
    my $suffix;
    if ($type eq 'alt-scaffold' || $type eq 'novel-patch') {
      $suffix = 'alt';
    } elsif ($type eq 'fix-patch') {
      $suffix = 'fix';
    }
    if ($suffix) {
      $aNameToSuffix{$name} = $suffix;
    }
  }
  close($F);
}

while (<>) {
  next if (/^#/);
  chomp;
  my @w = split("\t");
  my ($altScafName, $altAcc, $c, $cAcc, $altStart, $altEnd, $cStart, $cEnd, $altEndTail) =
       ($w[2], $w[3], $w[5], $w[6], $w[9], $w[10], $w[11], $w[12], $w[14]);
  my $suffix = $aNameToSuffix{$altScafName};
  if (! $suffix) {
    $suffix = ($altScafName =~ m/PATCH$/) ? "fix" : "alt";
  }
  my $chr = "chr$c";
  my $chrBase = $chr;
  if ($c =~ m/^HSCHR/) {
    # Mapping is not alt/fix to chrom, but fix to alt; convert alt accession to UCSC format:
    $chr = $cAcc;
    if ($opt_db && $opt_db eq 'hg19') {
      $chr =~ s/^(\w+)\.\d+/$1/ || die "Unexpected cAcc ($cAcc)";
      $chr = lc($chr);
    } else {
      $chr =~ s/^(\w+)\.(\d+)/$1v$2/ || die "Unexpected cAcc ($cAcc)";
    }
    $chrBase =~ s/HSCHR([A-Z0-9]+)_.*/$1/;
    $chr = "${chrBase}_${chr}_alt";
  }
  if ($opt_db && $opt_db eq 'hg19') {
    $altAcc =~ s/^(\w+)\.\d+/$1/ || die "Unexpected altAcc ($altAcc)";
    $altAcc = lc($altAcc);
  } else {
    $altAcc =~ s/^(\w+)\.(\d+)/$1v$2/ || die "Unexpected altAcc ($altAcc)";
  }
  $altAcc = "${chrBase}_${altAcc}_$suffix";
  my $altName = $altAcc;
  if ($altStart > 1 || $altEndTail > 0) {
    $altName .= ":$altStart-$altEnd";
  }
  print join("\t", $chr, $cStart-1, $cEnd, $altName) . "\n";
  print join("\t", $altAcc, $altStart-1, $altEnd, $chr.":$cStart-$cEnd") . "\n";
}
