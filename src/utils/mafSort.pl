#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 2 ) {
  printf STDERR "usage: mafSort.pl targetId file.maf > sorted.maf\n";
  printf STDERR "this sorts the maf blocks by the chromStart position of the target\n";
  printf STDERR "reference species. This is a two pass read of the input file.maf, thus it\n";
  printf STDERR "can not be a stdin reference, it must be an actual file, AND it can *not* be\n";
  printf STDERR "gzipped, it must be plain text.  The target reference species is expected\n";
  printf STDERR "to be found on the 's' line beginning:\n";
  printf STDERR "s targetId.chromName ...\n";
  printf STDERR "in the case of overlapping blocks, it will still be out sequence, and those\n";
  printf STDERR "lines will be noted.\n";
  exit 255;
}

sub vmPeak() {
  my $pid = $$;
  my $vmPeak = `grep -m 1 -i vmpeak /proc/$pid/status`;
  chomp $vmPeak;
  printf STDERR "# %s\n", $vmPeak;
}

my @header;	# first lines beginning with ^#
my @blocks;	# array index is block number starting at 0
                # value is the 'tell' byte position in the file where this
                # block starts
my %blockStarts;        # key is block number starting at 0
			# value is start position of this block
my %blockLength;        # key is block number starting at 0
                        # value is this block length
my $blockCount = 0;
my $prevEnd = 0;

my $targetId = shift;
my $inFile = shift;

open (FH, "<$inFile") or die "can not read $inFile";

while (my $line = <FH>) {
  chomp $line;
  if ($line =~ m/^#/) {
     if ($blockCount > 0) {
       printf STDERR "ERROR: why are there header lines after block $blockCount ?\n";
       printf STDERR "%s\n", $line;
       exit 255
     }
     push (@header, $line);
  } elsif ($line =~ m/^a /) {
     my $bytePosition = tell(FH);
     $bytePosition -= length($line) + 1;
     push (@blocks, $bytePosition);
  } elsif ($line =~ m/^s $targetId/) {
     my (undef, $chr, $s, $l, undef) = split('\s+', $line, 5);
     $chr =~ s/$targetId.//;
     $blockLength{$blockCount} = $l;
     $blockStarts{$blockCount++} = $s;
     if ($s < $prevEnd) {
        printf STDERR "out of sequence %d < %d at block %d\n", $s, $prevEnd, $blockCount;
     }
     $prevEnd = $s + $l;
  }
}
close (FH);

printf STDERR "read %d blocks in $inFile\n", $blockCount;

foreach my $headLine (@header) {
  printf "%s\n", $headLine;
}
$prevEnd = 0;
open (FH, "<$inFile") or die "can not read $inFile";
foreach my $blockId (sort { $blockStarts{$a} <=> $blockStarts{$b} } keys %blockStarts) {
  my $s = $blockStarts{$blockId};
  my $l = $blockLength{$blockId};
  my $bytePosition = $blocks[$blockId];
  seek(FH, $bytePosition, 0);
  my $line = <FH>;
  chomp $line;
  if ($line !~ m/^a /) {
     printf "ERROR: expected line to begin ^a, instead found:\n'%s'\n", $line;
     exit 255;
  }
  printf "%s\n", $line;
  while ($line = <FH>) {
    chomp $line;
    if (length($line) > 0) {
      printf "%s\n", $line;
    } else {
      last;
    }
  }
  printf "\n";
  
  if ($s < $prevEnd) {
     printf STDERR "still out of sequence %d < %d for blockId %d\n", $s, $prevEnd, $blockId;
  }
  $prevEnd = $s + $l;
}
close (FH);
vmPeak();
