#!/usr/bin/perl -w

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit ~/kent/src/hg/utils/automation/partitionSequence.pl instead.

# Based on Scott Schwartz's tclsh script make-joblist;
# rewritten in perl and extended to handle .2bit inputs and 
# multi-record fasta for target as well as query by Angie Hinrichs.
# Actual batch list creation can be done with gensub2 and a spec,
# using file lists created by this script.  

# $Id: partitionSequence.pl,v 1.1 2006/10/09 20:44:34 angie Exp $

use Getopt::Long;
use strict;

use vars qw/
    $opt_help
    $opt_oneBased
    $opt_concise
    $opt_xdir
    $opt_rawDir
    $opt_lstDir
    /;

sub usage {
  my ($status) = @_;
  my $base = $0;
  $base =~ s/^(.*\/)?//;
  print STDERR "
usage: $base chunk lap seqDir seqSizeFile seqLimit
    Partitions a set of sequences by size and prints out a list of file 
    specs suitable for creating a parasol job list with gensub2 and a job 
    template.  
    Divides large sequences up into chunk+lap-sized blocks at steps of chunk 
    (overlapping by lap).  For .2bit inputs, may combine small sequences into 
    .lst files in order to reduce batch size.  If small sequences are 
    combined into .lst files, then the -lstDir option must be given to 
    specify where to put the .lst files.  
    seqDir can be a single sequence file (e.g. \$db.2bit) or a directory 
    containing .fa(.gz) and/or .nib files or a .2bit file.  
    seqSizeFile (usually /cluster/data/\$db/chrom.sizes) must have sequence 
    names in its first column, and sequence sizes in its second column.  
    seqLimit is the maximum number of small sequences that may be lumped 
    together in the same partition.  It can be set to 0 for no limit.
options:
    -oneBased     Output 1-based start coords (for Scott's blastz-run)
    -concise      Don't add range specifiers to filenames unless they're split.
    -xdir script  Create a script with mkdir commands, one for each partition.
                  This is useful when two lists of partitions will be crossed 
                  in a cluster job, creating too many files for a single dir.
    -rawDir dir   The root dir for directories created in xdir.  Default: raw.
    -lstDir dir   When lumping small sequences together, put the lst files 
                  in dir.  dir should be nonexistent or empty before running 
                  this script (to avoid confusion caused by extranrous .lst's).
                  This option is required when partitioning a .2bit that 
                  contains sequences smaller than the chunk parameter!  
\n";
#"
  exit $status;
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
  # Look for .fa(.gz)/.nib/.2bit files in the given dir (can be a single file).
  my ($dir) = @_;
  if (-f $dir) {
    my @files = ($dir);
    return \@files;
  }
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

sub checkLstDir {
  # Make sure that we can start with an empty lstDir.
  if (! defined $opt_lstDir) {
    print STDERR "Error: -lstDir must be specified when small sequences " .
      "will be lumped together.\n";
    &usage(1);
  }
  system("mkdir -p $opt_lstDir");
  die "Error from mkdir -p $opt_lstDir: $!\n" if ($?);
  opendir(LSTDIR, $opt_lstDir) || die "Can't open lstDir $opt_lstDir: $!\n";
  my @existingFiles = grep { /[^\.]+/ } readdir(LSTDIR);
  closedir(LSTDIR);
  if (scalar(@existingFiles) != 0) {
    die "lstDir $opt_lstDir must be empty, but seems to have files " .
      " ($existingFiles[0] ...)\n";
  }
}

my $lstNum = 0;
sub makeLst {
  # Given a list of small sequence filespecs, create a .lst file containing 
  # the filespecs.
  my @specs = @_;
  my $lstBase = sprintf "part%03d", $lstNum++;
  my $lstFile = "$opt_lstDir/$lstBase.lst";
  open(LST, ">$lstFile") || die "Couldn't open $lstFile for writing: $!\n";
  foreach my $spec (@specs) {
    print LST "$spec\n";
  }
  close(LST);
  return ($lstBase, $lstFile);
}

sub partitionMonolith {
  # Given one seq file that contains *all* sequences for (target|query),
  # partition sequences into job specs using the hash of sequence sizes.  
  # Very large sequences (>$chunk+$lap) are split into overlapping chunks;
  # smaller sequences are glommed together to reduce the batch size.
  my ($sizesRef, $file, $chunk, $lap, $seqLimit) = @_;
  my @partitions = ();
  my $glomSize = 0;
  my @gloms = ();
  my $checkedLstDir = 0;
  my ($chr, $size);
  foreach my $csRef (@{&reverseSizeHash($sizesRef)}) {
    ($size, $chr) = @{$csRef};
    if ($size > $chunk + $lap) {
      # break it up
      my $intervalsRef = &overlappingIntervals(0, $size, $chunk, $lap);
      foreach my $i (@{$intervalsRef}) {
	my ($start, $end) = @{$i};
	push @partitions, ["$chr:$start-$end", "$file:$chr:$start-$end"];
      }
    } else {
      if (! $checkedLstDir) {
	&checkLstDir();
	$checkedLstDir = 1;
      }
      # glom it (but first see if it would make existing glom too big):
      if ($glomSize > 0 && (($glomSize + $size) > $chunk) || ((scalar(@gloms) >= $seqLimit) && ($seqLimit > 0))) {
	my ($lstBase, $lstFile) = &makeLst(@gloms);
	push @partitions, [$lstBase, $lstFile];
	$glomSize = 0;
	@gloms = ();
      }
      $glomSize += $size;
      my $spec = "$file:$chr";
      if (! $opt_concise) {
	$spec .= $opt_oneBased ? ":1-$size" : ":0-$size";
      }
      push @gloms, $spec;
    }
  }
  if ($glomSize > 0) {
    my ($lstBase, $lstFile) = &makeLst(@gloms);
    push @partitions, [$lstBase, $lstFile];
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
	push @partitions, ["$chr:$start-$end", "$file:$chr:$start-$end"];
      }
    } else {
      # one partition for this seq
      my $spec = $file;
      if (! $opt_concise) {
	$spec .= $opt_oneBased ? ":$chr:1-$size" : ":$chr:0-$size";
      }
      push @partitions, [$chr, $spec];
    }
  }
  return \@partitions;
}

sub partitionSeqs {
  my ($seqSizesRef, $filesRef, $chunk, $lap, $seqLimit) = @_;
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
      return &partitionMonolith($seqSizesRef, $file, $chunk, $lap, $seqLimit);
    } elsif ($file =~ /^(.*\/)?(.*)\.fa(\.gz)?$/) {
      my $root = $2;
      if (! exists $seqSizesRef->{$root}) {
	# This is a chunk file -- trust that it was already well-partitioned:
	push @partitions, [$root, "$file"];
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
		    "concise",
		    "xdir=s",
		    "rawDir=s",
		    "lstDir=s",
		    "help");
&usage(1) if (!$ok);
&usage(0) if ($opt_help);
&usage(1) if (scalar(@ARGV) != 5);
my ($chunk, $lap, $seqDir, $chromSizes, $seqLimit) = @ARGV;

my %sizes = %{&loadSeqSizes($chromSizes)};
my @files = @{&listSeqFiles($seqDir)};
my @partRefs = @{&partitionSeqs(\%sizes, \@files, $chunk, $lap, $seqLimit)};

$opt_rawDir = "raw" if (! defined $opt_rawDir);
if ($opt_xdir) {
  open(XDIR, ">$opt_xdir") || die "Couldn't open $opt_xdir for writing: $!\n";
  print XDIR "mkdir $opt_rawDir\n";
}
foreach my $partRef (@partRefs) {
  my ($part, $file) = @{$partRef};
  print "$file\n";
  if ($opt_xdir) {
    $file =~ s/(.*\/)?//;
    print XDIR "mkdir $opt_rawDir/$file\n";
  }
}
if ($opt_xdir) {
  close XDIR;
}
