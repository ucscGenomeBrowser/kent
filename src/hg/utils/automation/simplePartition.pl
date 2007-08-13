#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/simplePartition.pl instead.

# $Id: simplePartition.pl,v 1.3 2007/08/13 20:45:19 angie Exp $

use Getopt::Long;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;

# Option variable names:
use vars @HgAutomate::commonOptionVars;

# Option defaults:
my $defaultWorkhorse = 'least loaded';

my $maxDirSize = 1000;

my $base = $0;
$base =~ s/^(.*\/)?//;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base db.2bit chunkSize partitionDir
options:
";
#  print STDERR <<_EOF_
#    -buildDir dir         Use dir instead of default
#                          $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/template.\$date
#                          (necessary when continuing at a later date).
#_EOF_
#  ;
  print STDERR &HgAutomate::getCommonOptionHelp(
					'workhorse' => $defaultWorkhorse);
  print STDERR "
Given a .2bit file and a chunk size, create a directory structure with
no more than $maxDirSize subdirectories per level.  The leaf directories each
contain a single .lst file containing one or more .2bit specs and a
single .lft liftUp spec file.  This allows room in the leaf
directories for multiple other files that may be created during a
batch run.  Directories are named using fixed-width sequential numbers
so that chunk order is preserved with wildcard expansion, for easy
concatenation of batch results.  A file partitionDir/levels is
created, containing the number of levels of the directory structure
including partitionDir through leaf directories.  A file
partitionDir/partitions.lst is created, containing one relative path
(in partitionDir) to each .lst file, for batch spec generation
(e.g. using gensub2).  Chunks are non-overlapping.  partitionDir must
be empty or nonexistent before this script is invoked.
";
  # Detailed help (-help):
  print STDERR "
" if ($detailed);
  print "\n";
  exit $status;
} # usage


# Globals:
# Command line args:
my ($twoBit, $chunkSize, $partitionDir);
# Other:

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgAutomate::commonOptionSpec,
		      );
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  &HgAutomate::processCommonOptions();
}


sub checkEmpty {
  # Die if $dir already exists and is not an empty dir.
  my ($dir) = @_;
  if (-e $dir) {
    if (-d $dir) {
      if (`ls -1 $dir/ | wc -l` > 0) {
	die "\nError: $dir is not empty.  Please clean it up and run again.\n";
      }
    } else {
      die "\nError: $dir seems to be a file not a directory.\n";
    }
  }
}

sub getParts {
  # Return a 3-level list(ref): partitions -> chunks -> chunk properties.
  my @parts = ();
  my $cmd = "twoBitInfo $twoBit stdout |";
  &HgAutomate::verbose(2, "Making \@parts from 2bit...\n");
  open(P, $cmd)
    || die "Couldn't open pipe ($cmd): $_\n";
  my @glomChunks = ();
  my $glomSize = 0;
  while (<P>) {
    chomp;
    my ($seq, $seqSize) = split("\t");
    if ($glomSize > 0 && ($glomSize + $seqSize) > $chunkSize) {
      # New sequence doesnt fit in glom -- flush out the glom.
      my @glomChunksCopy = @glomChunks;
      push @parts, \@glomChunksCopy;
      @glomChunks = ();
      $glomSize = 0;
    }
    if ($seqSize > $chunkSize) {
      # Break into chunks.
      for (my $offset = 0;  $offset < $seqSize;  $offset += $chunkSize) {
	my $end = $offset + $chunkSize;
	$end = $seqSize if ($end > $seqSize);
	my $size = $end - $offset;
	my $chunkSpec = [ $seq, $seqSize, $offset, $size,
			  "$twoBit:$seq:$offset-$end" ];
	my @partChunks = ( $chunkSpec );
	push @parts, \@partChunks;
      }
    } else {
      # Add to glom.
      my $chunkSpec = [ $seq, $seqSize, 0, $seqSize, "$twoBit:$seq" ];
      push @glomChunks, $chunkSpec;
      $glomSize += $seqSize;
    }
  }
  if ($glomSize > 0) {
    # Flush out last glom.
    my @glomChunksCopy = @glomChunks;
    push @parts, \@glomChunksCopy;
  }
  close(P);
  return \@parts;
} # getParts

sub partPathFromNum {
  # Given a partition ID number and number of levels in directory tree,
  # determine its path and base filename.
  my ($partNum, $levels) = @_;
  my $leafId = $partNum % $maxDirSize;
  my $partMod = int($partNum / $maxDirSize);
  my $path = sprintf "%03d", $leafId;
  for (my $i = 1;  $i < $levels;  $i++) {
    $path = (sprintf "%03d/", ($partMod % $maxDirSize)) . $path;
    $partMod = int($partMod / $maxDirSize);
  }
  my $partName = $path;
  $partName =~ s@/@@g;
  return ($path, $partName);
} # partPathFromNum

sub simplePartition {
  my @parts = @{&getParts()};
  my $numParts = scalar(@parts);
  my $levels = 1 + int(log($numParts) / log($maxDirSize));
  &HgAutomate::verbose(2, "Got $numParts partitions, $levels levels\n");

  # Write number of levels into $partitionDir/levels so calling script knows
  # how many levels to traverse...
  &HgAutomate::mustMkdir($partitionDir);
  my $fh = &HgAutomate::mustOpen(">$partitionDir/levels");
  print $fh "$levels\n";
  close($fh);

  # Create directory structure and populate leaf dirs.
  &HgAutomate::verbose(2, "Creating directory tree and .lst,.lft files\n");
  $fh = &HgAutomate::mustOpen(">$partitionDir/partitions.lst");
  my $partNum = 0;
  foreach my $partChunks (@parts) {
    my ($path, $partName) = &partPathFromNum($partNum, $levels);
    &HgAutomate::mustMkdir("$partitionDir/$path");
    my $lstFh = &HgAutomate::mustOpen(">$partitionDir/$path/$partName.lst");
    my $lftFh = &HgAutomate::mustOpen(">$partitionDir/$path/$partName.lft");
    foreach my $chunkSpec (@{$partChunks}) {
      my ($seq, $seqSize, $offset, $size, $spec) = @{$chunkSpec};
      print $lstFh "$spec\n";
      my $specName = $spec;
      $specName =~ s/^[^:]+://;
      print $lftFh "$offset\t$specName\t$size\t$seq\t$seqSize\n";
    }
    close($lstFh);
    close($lftFh);
    print $fh "$path/$partName.lst\n";
    $partNum++;
  }
  close($fh);
} # simplePartition


#########################################################################
# main

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

# Make sure we have valid options and exactly 3 arguments:
&checkOptions();
&usage(1) if (scalar(@ARGV) != 3);
($twoBit, $chunkSize, $partitionDir) = @ARGV;

# Force debug and verbose until this is looking pretty solid:
$opt_debug = 1;
$opt_verbose = 3 if ($opt_verbose < 3);

&checkEmpty($partitionDir);

&simplePartition();

&HgAutomate::verbose(1,
	"\n *** All done!\n");
&HgAutomate::verbose(1, "\n");

