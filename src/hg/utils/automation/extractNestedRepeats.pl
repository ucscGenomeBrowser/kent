#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/extractNestedRepeats.pl instead.

# $Id: extractNestedRepeats.pl,v 1.5 2007/09/20 07:42:30 angie Exp $

use Getopt::Long;
use warnings;
use strict;
use Carp;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;

# Option variable names, both common and peculiar to this script:
use vars @HgAutomate::commonOptionVars;
use vars qw/ $opt_rolloverFudge
           /;

my $base = $0;
$base =~ s/^(.*\/)?//;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base rmsk.out [another.out ...]
Uses the ID (15th) column of RepeatMasker output to join up fragments of
repeats that have been interrupted by subsequent insertions (nested repeats).
Output is BED 12+, written to stdout.
";
}

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgAutomate::commonOptionSpec,
		      'rolloverFudge=i',
		     );
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  &HgAutomate::processCommonOptions();
}


sub getStrands {
  # Make a ,-list of the blocks' strands, and pick an overall strand:
  # +, - or . for mixed.
  my ($grpRef) = @_;
  my @fragmentRefs = @{$grpRef};
  my $overallStrand;
  my $blkStrands = "";
  foreach my $fRef (@fragmentRefs) {
    my $strand = $fRef->[3];
    if (! $overallStrand) {
      $overallStrand = $strand;
    } elsif ($overallStrand ne $strand) {
      $overallStrand = '.';
    }
    $blkStrands .= $strand . ',';
  }
  return ($overallStrand, $blkStrands);
}


sub mostCommonName {
  # Block names are not always identical -- but if one appears more often
  # than others, use it.
  my ($grpRef) = @_;
  my @fragmentRefs = @{$grpRef};
  my %nameCounts;
  foreach my $fRef (@fragmentRefs) {
    my $name = $fRef->[4];
    my $class = $fRef->[5];
    my $family;
    if ($class =~ s@/(.*)@@) {
      $family = $1;
    } else {
      $family = $class;
    }
    $nameCounts{"$name|$class|$family"}++;
  }
  my @sortedByCount = sort { $nameCounts{$b} <=> $nameCounts{$a}}
    keys %nameCounts;
  return split(/\|/, $sortedByCount[0]);
}


sub writeGroups {
  # Take a listRef indexed by id of listRefs describing repeats in the group,
  # make a bed12 row for the group and write it to stdout.
  my ($groupedRef, $warnSkipped, $offset) = @_;

  for (my $id = 1;  $id < @{$groupedRef};  $id++) {
    my $realId = $id + $offset;
    if (! $groupedRef->[$id]) {
      print STDERR "Skipped id $realId (in lines preceding $.)\n"
	if ($warnSkipped);
      next;
    }
    # Merge overlapping blocks, remove completely-contained blocks:
    my @fragmentRefs;
    my $prevBlkEnd;
    my $totalScore = 0;
    foreach my $fRef (@{$groupedRef->[$id]}) {
      $fRef->[1] = $prevBlkEnd
	if (defined $prevBlkEnd && $prevBlkEnd > $fRef->[1]);
      next unless ($fRef->[2] > $fRef->[1]);
      push @fragmentRefs, $fRef;
      $totalScore += $fRef->[6];
      $prevBlkEnd = $fRef->[2];
    }
    if (@fragmentRefs > 1) {
      my $chrom = $fragmentRefs[0]->[0];
      my $chromStart = $fragmentRefs[0]->[1];
      my $chromEnd = $fragmentRefs[-1]->[2];
      my ($name, $class, $family) = &mostCommonName(\@fragmentRefs);
      # Average the scores, to assign a shade similar to the fragments in
      # the main RepeatMasker track:
      my $score = int(0.5 + $totalScore / scalar(@fragmentRefs));
      my ($strand, $blkStrands) = &getStrands(\@fragmentRefs);
      my $blkCount = @fragmentRefs;
      my ($blkSizes, $blkStarts) = ('', '');
      foreach my $fRef (@fragmentRefs) {
	$blkSizes  .= ($fRef->[2] - $fRef->[1]) . ',';
	$blkStarts .= ($fRef->[1] - $chromStart) . ',';
      }
      print "$chrom\t$chromStart\t$chromEnd\t$name\t$score\t$strand\t" .
	"$chromStart\t$chromEnd\t0\t$blkCount\t$blkSizes\t$blkStarts\t" .
	  "$blkStrands\t$realId\t$class\t$family\n";
    }
  }
}

#########################################################################
# main

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

# Make sure we have valid options and exactly 1 argument:
&checkOptions();
&usage(1) if (scalar(@ARGV) < 1);

my $prevChr;
my $prevId = 0;
my @groupedById = ();
my $offset = 0;
my $idProblems = 0;
while (<>) {
  # Skip header lines:
  next if (/^(   SW.*|score.*|\s*)$/);
  chomp;
  my @row = split;
  my ($chr, $start, $end, $strand, $repName, $repClassFam, $id,
      $percDiv, $percDel, $percIns) =
    ($row[4], $row[5], $row[6], $row[8], $row[9], $row[10], $row[14],
     $row[1], $row[2], $row[3]);
  if (! defined $id) {
    warn "RepeatMasker bug?: Undefined id, line $. of input:\n$_\n";
    $idProblems++;
    next;
  } elsif ($id !~ /^\d+$/) {
    warn "RepeatMasker bug?: Non-numeric id \"$id\", line $. of input:\n$_\n";
    $idProblems++;
    next;
  }
  $start--;
  $strand = '-' if ($strand eq 'C');
  # If we are starting a new chrom, or ID's have rolled over due to
  # concatenation of .out's without lifting ID's, settle accounts:
  my $chrRollover = ($prevChr && $chr ne $prevChr);
  if ($chrRollover ||
      ($opt_rolloverFudge && $id == 1 && $prevId > $opt_rolloverFudge)) {
    &writeGroups(\@groupedById, !$chrRollover, $offset);
    @groupedById = ();
    $offset = $id;
  }
  # Calc 0-1000 BED score to match the shading in hgTracks/rmskTrack.c.
  # rmskTrack.s maps the whole grayscale dynamic range to percId 500-1000.
  # "percId" should really be called "milliId", but I'll stick with the
  # rmskTrack.c variable name.
  # (actually, the 10*$percX --> milliX step is performed in hgLoadOut):
  my $percId = 1000 - 10*$percDiv - 10*$percDel - 10*$percIns;
  # Map 500-1000 Id score dynamic range onto 0-1000 bed score range:
  $percId = 500 if ($percId < 500);
  $percId = 1000 if ($percId > 1000);
  my $bedScore = ($percId - 500) * 2;
  push @{$groupedById[$id-$offset]},
    [$chr, $start, $end, $strand, $repName, $repClassFam, $bedScore];
  $prevId = $id;
  $prevChr = $chr;
}
&writeGroups(\@groupedById, 0, $offset);

if ($idProblems) {
  die "At least one ID was missing (see warnings above) -- " .
    "please report to Robert Hubley.  -continue at your disgression.\n";
}
