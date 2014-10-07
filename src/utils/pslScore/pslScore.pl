#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc < 1) {
  printf STDERR "pslScore - calculate web blat score from psl files\n";
  printf STDERR "usage:\n";
  printf STDERR "  pslScore.pl <file.psl> [moreFiles.psl]\n";
  printf STDERR "options:\n";
  printf STDERR "   none at this time\n";
  exit 255;
}

# pslIsProtein() C code:
# return  (((psl->strand[1] == '+' ) &&
#    (psl->tEnd == psl->tStarts[lastBlock] + 3*psl->blockSizes[lastBlock])) ||
#   ((psl->strand[1] == '-') &&
#    (psl->tStart == (psl->tSize-(psl->tStarts[lastBlock] + 3*psl->blockSizes[lastBlock])))));

# is psl a protein psl (are it's blockSizes and scores in protein space)
# 1 == not protein, return 3 == protein
sub pslIsProtein($$$$$$$) {
  my ($blockCount, $strand, $tStart, $tEnd, $tSize, $tStarts, $blockSizes) = @_;
  my @starts = split(',', $tStarts);
  my @sizes = split(',', $blockSizes);
  my $lastBlock = $blockCount - 1;
  my $answer = 1;
  if ($strand =~ m/^\+/) {
    my $test = $starts[$lastBlock] + (3 * $sizes[$lastBlock]);
    $answer = 3 if ($tEnd == $test);
  } elsif ($strand =~ m/^-/) {
    my $test = $tSize - ($starts[$lastBlock] + (3 * $sizes[$lastBlock]));
    $answer = 3 if ($tStart == $test);
  }
  return $answer;
} # pslIsProtein()

# int pslCalcMilliBad(struct psl *psl, boolean isMrna)
# /* Calculate badness in parts per thousand. */
# {
# int sizeMul = pslIsProtein(psl) ? 3 : 1;
# int qAliSize, tAliSize, aliSize;
# int milliBad = 0;
# int sizeDif;
# int insertFactor;
# int total;
# 
# qAliSize = sizeMul * (psl->qEnd - psl->qStart);
# tAliSize = psl->tEnd - psl->tStart;
# aliSize = min(qAliSize, tAliSize);
# if (aliSize <= 0)
#     return 0;
# sizeDif = qAliSize - tAliSize;
# if (sizeDif < 0)
#     {
#     if (isMrna)
#         sizeDif = 0;
#     else
#         sizeDif = -sizeDif;
#     }
# insertFactor = psl->qNumInsert;
# if (!isMrna)
#     insertFactor += psl->tNumInsert;
# 
# total = (sizeMul * (psl->match + psl->repMatch + psl->misMatch));
# if (total != 0)
#     milliBad = (1000 * (psl->misMatch*sizeMul + insertFactor + round(3*log(1+sizeDif)))) / total;
# return milliBad;
# }

sub pslCalcMilliBad($$$$$$$$$$$) {
my ($sizeMul, $qEnd, $qStart, $tEnd, $tStart, $qNumInsert, $tNumInsert, $matches, $repMatches, $misMatches, $isMrna) = @_;
my $milliBad = 0;
my $qAliSize = $sizeMul * ($qEnd - $qStart);
my $tAliSize = $tEnd - $tStart;
my $aliSize = $qAliSize;
$aliSize = $tAliSize if ($tAliSize < $qAliSize);
if ($aliSize <= 0) {
  return $milliBad;
}
my $sizeDif = $qAliSize - $tAliSize;
if ($sizeDif < 0) {
  if ($isMrna) {
      $sizeDif = 0;
  } else {
      $sizeDif = -$sizeDif;
  }
}
my $insertFactor = $qNumInsert;
if (0 == $isMrna) {
  $insertFactor += $tNumInsert;
}
my $total = ($sizeMul * ($matches + $repMatches + $misMatches));
if ($total != 0) {
  my $roundAwayFromZero = 3*log(1+$sizeDif);
  if ($roundAwayFromZero < 0) {
    $roundAwayFromZero = int($roundAwayFromZero - 0.5);
  } else {
    $roundAwayFromZero = int($roundAwayFromZero + 0.5);
  }
  $milliBad = (1000 * ($misMatches*$sizeMul + $insertFactor + $roundAwayFromZero)) / $total;
}
return $milliBad;
} # sub pslCalcMilliBad()

while (my $file = shift) {
  if ($file =~ m/.gz$/) {
    open (FH, "zcat $file|") or die "can not read $file";
  } else {
    open (FH, "<$file") or die "can not read $file";
  }
  while (my $line = <FH>) {
    next if ($line =~ m/^#/);
    chomp $line;
    my ($matches, $misMatches, $repMatches, $nCount, $qNumInsert, $qBaseInsert, $tNumInsert, $tBaseInsert, $strand, $qName, $qSize, $qStart, $qEnd, $tName, $tSize, $tStart, $tEnd, $blockCount, $blockSizes, $qStarts, $tStarts) = split('\t', $line);
    my $sizeMul = pslIsProtein($blockCount, $strand, $tStart, $tEnd, $tSize, $tStarts, $blockSizes);
    my $pslScore = $sizeMul * ($matches + ( $repMatches >> 1) ) -
        $sizeMul * $misMatches - $qNumInsert - $tNumInsert;
    my $milliBad = int(pslCalcMilliBad($sizeMul, $qEnd, $qStart, $tEnd, $tStart, $qNumInsert, $tNumInsert, $matches, $repMatches, $misMatches, 1));
    my $percentIdentity = 100.0 - $milliBad * 0.1;
    printf "%s\t%d\t%d\t%s:%d-%d\t%d\t%.2f\n", $tName, $tStart, $tEnd, $qName, $qStart, $qEnd, $pslScore, $percentIdentity;

  }
  close (FH);
}
