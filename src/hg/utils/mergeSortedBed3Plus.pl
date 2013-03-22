#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/mergeSortedBed3Plus.pl instead.

use Getopt::Long;
use warnings;
use strict;

use vars qw/
    $opt_maxGap
    $opt_help
    /;

my $ok = GetOptions('maxGap=n', 'help');
if (!$ok || $opt_help || scalar(@ARGV) < 1) {
  die <<EOF
usage: mergeSortedBed3Plus.pl [-maxGap=N] inputFile

Expects BED input with at least 3 fields, sorted by {chrom, chromStart}
(e.g. by the command 'sort -k1,1 -k2n,2n').  When consecutive lines
have identical content from the fourth to last column, but adjacent or
overlapping {chrom, chromStart, chromEnd} ranges, this will print one
line with the merged range (BED3) followed by the identical content.
inputFile can be - or stdin to read from stdin.

options:
  -maxGap=N     Bridge gaps between adjacent items of at most N bases.
                Default: 0 (no gaps)

EOF
}

grep {s/^stdin$/-/i} @ARGV;
my $maxGap = defined $opt_maxGap ? $opt_maxGap : 0;


my ($mergeChr, $mergeStart, $mergeEnd, $mergeContent);

sub printOne() {
  print join("\t", $mergeChr, $mergeStart, $mergeEnd, $mergeContent) . "\n";
}

while (<>) {
  next if (/^\s*#/ || /^\s*$/);
  m/^(\w+)\s+(\d+)\s+(\d+)\s*(.*)$/ || die "Unrecognized format (expecting at least BED3):\n$_\t";
  my ($chr, $start, $end, $content) = ($1, $2, $3, $4);
  if (! defined $mergeChr
      || $chr ne $mergeChr
      || $mergeEnd + $maxGap < $start
      || $content ne $mergeContent) {
    if (defined $mergeChr) {
      printOne();
    }
    ($mergeChr, $mergeStart, $mergeEnd, $mergeContent) = ($chr, $start, $end, $content);
  } else {
    if ($start < $mergeStart) {
      die "Input is not sorted! (chr = $chr, $start < $mergeStart)"
    }
    $mergeEnd = $end if ($end > $mergeEnd);
  }
}

printOne() if (defined $mergeChr);
