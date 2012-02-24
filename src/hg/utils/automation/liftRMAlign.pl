#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/liftRMAlign.pl instead.

# $Id: liftRMAlign.pl,v 1.5 2007/07/18 17:11:48 angie Exp $

use warnings;
use strict;

my $basename = $0;
$basename =~ s@.*/@@;

my $suffix = "fa.align";

sub usage {
  my $status = shift;
  print STDERR "
usage: $basename liftSpec

This reads liftSpec, then looks for files under the current directory
named <chunk>.$suffix where <chunk> is each chunk named in liftSpec.
Lifts up coordinates in each <chunk>.$suffix and prints to STDOUT.

";
  exit($status);
}

#
# readLift: read the given liftFile into %liftSpec and @orderedChunks.
#
# liftSpec hash, indexed by chunk name (oldName):
my %liftSpec = ();
# ordered list of chunk names:
my @orderedChunks = ();
sub readLift {
  my $liftFile = shift;
  open(LIFT, "$liftFile")
    || die "Couldn't open $liftFile: $!\n";
  while (<LIFT>) {
    my @words = split(/\s/);
    my $chunk = $words[1];
    $liftSpec{$chunk}->{offset}  = $words[0];
    $liftSpec{$chunk}->{oldSize} = $words[2];
    $liftSpec{$chunk}->{newName} = $words[3];
    $liftSpec{$chunk}->{newSize} = $words[4];
    push @orderedChunks, $chunk;
  }
  close(LIFT);
}

#
# updateSpacing: make sure alignment lines have consistent spacing, 
# and specify the number of spaces to add if necessary (i.e. when we're 
# expanding Arian's/cross_match's 6-character coords into 9-character).  
#
my $leftCharCount;
my $extraSpaces = "";
sub updateSpacing {
  my $leftStr = shift;
  my $newLCC  = length($leftStr);

  if (defined $leftCharCount) {
    if ($leftCharCount != $newLCC) {
      die "Error: inconsistent alignment line spacing in input.  Previously saw a length of $leftCharCount, now I see a length of $newLCC at line $..";
    }
  } else {
    $leftCharCount = $newLCC;
    my $extraSpaceCount = 25 - $newLCC;
    if ($extraSpaceCount < 0) {
      die "Error: input seems to have wider alignment lines than expected!";
    }
    for (my $i=0;  $i < $extraSpaceCount;  $i++) {
      $extraSpaces .= " ";
    }
  }
}


###########################################################################
# MAIN
#

&usage(1) if (scalar(@ARGV) != 1);
&usage(0) if ($ARGV[0] =~ /^-h/);

my $liftFile = $ARGV[0];
&readLift($liftFile);

foreach my $chunk (@orderedChunks) {
  my $chunkFile = "$chunk.$suffix";
  my $newName = $liftSpec{$chunk}->{newName};
  my $offset  = $liftSpec{$chunk}->{offset};
  my $newSize = $liftSpec{$chunk}->{newSize};
  my $oldName = $chunk;
  my $oldNameSuffix = "";
  if ($oldName =~ s/(-\d+)$//) {
    $oldNameSuffix = $1;
  }
  $oldName =~ s@.*/@@;
  if (! open (CHUNK, "$chunkFile")) {
    print STDERR "FYI Couldn't open $chunkFile: $!\n";
    next;
  }
  while (<CHUNK>) {
    if (/\s+$oldName($oldNameSuffix)?\s+(\d+)\s+(\d+)\s+\((\d+)\)/) {
      # .out-like header line
      my ($oldStart, $oldEnd, $oldLeft) = ($2, $3, $4);
      my $newStart = $oldStart + $offset;
      my $newEnd   = $oldEnd   + $offset;
      my $newLeft  = $newSize  - $newEnd;
      s/(\s+)$oldName($oldNameSuffix)?(\s+)\d+(\s+)\d+(\s+)\(\d+\)/$1$newName$3$newStart$4$newEnd$5($newLeft)/;
    } elsif (/^(\s+)$oldName($oldNameSuffix)?(\s+)(\d+)(\s+\S+\s+)(\d+)\s*$/) {
      # chunk sequence line
      my ($initSpace, $maybeSuffix, $leftSpace, $oldStart, $seqStuff, $oldEnd) =
	($1, $2, $3, $4, $5, $6);
      my $newStart = $oldStart + $offset;
      my $newEnd   = $oldEnd   + $offset;
      if (defined($maybeSuffix)) {
         &updateSpacing("$oldName$maybeSuffix$leftSpace$oldStart");
      } else {
         &updateSpacing("$oldName$leftSpace$oldStart");
      }
      $_ = sprintf("$initSpace%-15s %9d%s%-9d\n",
		   $newName, $newStart, $seqStuff, $newEnd);
    } elsif (/^(C\s+|\s+)(\S+)(\s+)(-?\d+)(\s+\S+\s+)(-?\d+)\s*$/) {
      # repeat sequence line -- reformat to use larger coords like chunk.
      my ($initSpace, $repName, $leftSpace, $repStart, $seqStuff, $repEnd) =
	($1, $2, $3, $4, $5, $6);
      &updateSpacing("$repName$leftSpace$repStart");
      $_ = sprintf("$initSpace%-15s %9d%s%-9d\n",
		   $repName, $repStart, $seqStuff, $repEnd);
    } elsif (/^([ iv\-\?]+)$/) {
      # alignment line -- reformat to allow for larger coords
      $_ = "$extraSpaces$1\n";
    } elsif (/^(Transitions|Gap_init|Assumed|Matrix|#|RepeatMasker|run with|RepBase)/) {
      # info lines, do nothing
    } elsif (/\S/) {
      # non-blank line with something we haven't recognized already
      die "unrecognized format, line $. of $chunkFile:\n$_\n";
    }
    print;
  } # each line of chunkFile
  close(CHUNK);
} # each chunk
