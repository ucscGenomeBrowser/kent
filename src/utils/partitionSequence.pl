#!/usr/bin/perl -w

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit ~/kent/src/utils/partitionSequence.pl instead.

# Based on Scott Schwartz's tclsh script make-joblist;
# rewritten in perl and extended to handle .2bit inputs and 
# multi-record fasta for target as well as query by Angie Hinrichs.

# $Id: partitionSequence.pl,v 1.2 2005/01/24 18:54:55 angie Exp $

use Getopt::Long;
use strict;

use vars qw/
    $opt_help
    $opt_oneBased
    /;

sub usage {
  die "\nusage: $0 chunk lap seqDir chromSizes\n" .
      "    Divides large sequences up into chunk+lap-sized blocks at \n" .
      "    steps of chunk (overlapping by lap).  For .2bit inputs, \n" .
      "    may combine small sequences into .lst files in order to reduce \n" .
      "    batch size.\n" .
      "options:\n" .
      "    -oneBased   output 1-based start coords (for blastz)\n" .
      "\n";
}

sub loadSeqSizes {
  # Read sequence names and sizes from a file such as chrom.sizes.
  my ($seqSizeFile) = @_;
  open(SEQ, "$seqSizeFile") || die "Can't open seq size file $seqSizeFile: $!\n";
  my %sizes;
  while (<SEQ>) {
    chomp;
    my ($seq, $size) = split;
    $sizes{$seq} = $size;
  }
  close(SEQ);
  return \%sizes;
}

sub listSeqFiles {
  # Look for .fa(.gz)/.nib/.2bit files in the given dir.
  my ($dir) = @_;
  opendir(SEQDIR, $dir) || die "Can't ls $dir: $!\n";
  my @files = grep { /\.(nib|fa|2bit)$/ } readdir(SEQDIR);
  if (scalar(grep { /\.2bit/ } @files) > 1) {
    die "Sorry, $0 only supports one .2bit file per target/query but found multiple .2bit files in $dir";
  }
  closedir(SEQDIR);
  foreach my $f (@files) {
    $f = "$dir/$f";
  }
  return \@files;
}


sub reverseSizeHash {
  # Sort hash by value (descending numeric).
  my ($hRef) = @_;
  my @sizeChroms = ();
  # Got this trick from "man perlfunc", see description of keys:
  foreach my $seq (sort { $hRef->{$b} <=> $hRef->{$a} } keys %{$hRef}) {
    push @sizeChroms, [$hRef->{$seq}, $seq];
  }
  return \@sizeChroms;
}

sub overlappingIntervals {
  # Return a list of overlapping intervals (1-based starts if $opt_oneBased).
  # Starts increment by $chunk; each end overlaps the next start by $lap.
  my ($start, $end, $chunk, $lap) = @_;
  my @intervals = ();
  for (my $iStart = $start;  $iStart < $end;  $iStart += $chunk) {
    my $iEnd = $iStart + $chunk + $lap;
    $iEnd = $end if ($iEnd > $end);
    push @intervals, [($opt_oneBased ? $iStart + 1 : $iStart), $iEnd];
  }
  return \@intervals;
}

sub partitionMonolith {
  # Given one seq file that contains *all* sequences for (target|query),
  # partition sequences into job specs using the hash of sequence sizes.  
  # Very large sequences (>$chunk+$lap) are split into overlapping chunks;
  # smaller sequences are glommed together to reduce the batch size.
  my ($sizesRef, $file, $chunk, $lap) = @_;
  my @partitions = ();
  my $glomSize = 0;
  my $gloms = "";
  my ($chr, $size);
  foreach my $csRef (@{&reverseSizeHash($sizesRef)}) {
    ($size, $chr) = @{$csRef};
    if ($size > $chunk + $lap) {
      # break it up
      my $intervalsRef = &overlappingIntervals(0, $size, $chunk, $lap);
      foreach my $i (@{$intervalsRef}) {
	my ($start, $end) = @{$i};
	push @partitions, [$chr, $start, $end, "$file:$chr:$start-$end"];
      }
    } else {
      # glom it (but first see if it would make existing glom too big):
      if ($glomSize > 0 && ($glomSize + $size) > $chunk) {
	$gloms =~ s/,$//;
	push @partitions, [$chr, 0, 0, $gloms];
	$glomSize = 0;
	$gloms = "";
      }
      $glomSize += $size;
      $gloms .= "$file:$chr,";
    }
  }
  if ($glomSize > 0) {
    $gloms =~ s/,$//;
    push @partitions, [$chr, 0, 0, $gloms];
  }
  return \@partitions;
}

sub partitionPerSeqFiles {
  # Given a list of per-sequence files, break up very large (>$chunk+$lap) 
  # sequences into overlapping chunks; otherwise make one job per seq.
  my ($seqSizesRef, $filesRef, $chunk, $lap) = @_;
  my @partitions = ();
  foreach my $file (@{$filesRef}) {
    my $chr = $file;
    $chr =~ s/^(.*\/)?(.*)\.\w+(\.gz)?$/$2/;
    my $size = $seqSizesRef->{$chr};
    die "No seq size for $chr" if (! defined $size);
    if ($size > $chunk + $lap) {
      # break it up
      my $intervalsRef = &overlappingIntervals(0, $size, $chunk, $lap);
      foreach my $i (@{$intervalsRef}) {
	my ($start, $end) = @{$i};
	push @partitions, [$chr, $start, $end, "$file:$chr:$start-$end"];
      }
    } else {
      # one partition for this seq
      push @partitions, [$chr, 0, 0, $file];
    }
  }
  return \@partitions;
}

sub partitionSeqs {
  my ($seqSizesRef, $filesRef, $chunk, $lap) = @_;
  my @partitions = ();
  my @seqFiles = ();
  foreach my $file (@{$filesRef}) {
    if ($file =~ /\.2bit$/) {
      if (scalar(@{$filesRef}) != 1) {
	die "Sorry, if .2bit is used it must be the only file for its species";
	# We could be more flexible here and assume that multiple .2bit files 
	# mean pre-partitioning like chunked .fa, but I don't see a demand 
	# so won't implement and test that at this point.
      }
      return &partitionMonolith($seqSizesRef, $file, $chunk, $lap);
    } elsif ($file =~ /^(.*\/)?(.*)\.fa(\.gz)?$/) {
      my $root = $2;
      if (! exists $seqSizesRef->{$root}) {
	# This is a chunk file -- trust that it was already well-partitioned:
	push @partitions, [$root, 0, 0, "$file"];
      } else {
	push @seqFiles, $file;
      }
    } elsif ($file =~ /\.nib$/) {
      push @seqFiles, $file;
    } else {
      die "Unrecognized file type: $file"
    }
  }
  if (scalar(@seqFiles) > 0) {
    push @partitions, @{&partitionPerSeqFiles($seqSizesRef, \@seqFiles,
					      $chunk, $lap)};
  }
  return \@partitions;
}


#########################################################################
#
# -- main --

my $ok = GetOptions("oneBased",
		    "help");
&usage() if (!$ok || $opt_help);
&usage() if (scalar(@ARGV) != 4);
my ($chunk, $lap, $seqDir, $chromSizes) = @ARGV;

my %sizes = %{&loadSeqSizes($chromSizes)};
my @files = @{&listSeqFiles($seqDir)};
my @chunks = @{&partitionSeqs(\%sizes, \@files, $chunk, $lap)};

foreach my $chunk (@chunks) {
  my ($seq, $start, $end, $file) = @{$chunk};
  print "$file\n";
}

