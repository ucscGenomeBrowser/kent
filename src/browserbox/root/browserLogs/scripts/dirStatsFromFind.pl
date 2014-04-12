#!/usr/bin/env perl

use warnings;
use strict;
# use File::stat;
use Math::BigInt;

my $prog=`basename $0`;
chomp $prog;
my $argc = scalar(@ARGV);
my $blockSize = 0;
my $oneHundred = Math::BigInt->new(100);
my $oneK = Math::BigInt->new(1024);
my $tmpDir = "/var/tmp";
if ( ! -d "/var/tmp" ) {
   $tmpDir = "/tmp";
}
if (defined($ENV{'TMPDIR'})) {
  $tmpDir = $ENV{'TMPDIR'};
}

##########################################################################
sub usage()
{
printf "usage: %s <findResultFile.txt> <directoryName> [other directory names]\n", $prog;
printf "\twill produce statistics on file sizes in the given directory\n";
printf "\tname list from the file name list provided.\n";
printf "\tBy default the 'root' directory will be counted without being.\n";
printf "\ton the list of directories to analyze.\n";
}

##########################################################################
sub realSize($$) {
  my ($apparentSize, $percentDensity) = @_;
  if ($percentDensity < 100) {
    my $tRealSize = Math::BigInt->new($percentDensity);
    $tRealSize->bmul($apparentSize);
    $tRealSize->bdiv($oneHundred);
    return $tRealSize->bstr();
  } else {
    return $apparentSize->bstr();
  }
}

##########################################################################
sub percentDensity($$$) {
  my ($apparentSize, $blockCount, $blockSize) = @_;
  my $tBs = Math::BigInt->new($blockSize);
  my $tBc = Math::BigInt->new($blockCount);
  $tBs->bmul($tBc);
  my $percentOccupied = Math::BigInt->new($tBs);
  $percentOccupied->bmul($oneHundred);
  if ($apparentSize->is_pos()) {
      $percentOccupied->bdiv($apparentSize);
  } else {
     $percentOccupied = Math::BigInt->new(0);
  }
# printf STDERR "test: (%d * %d) / %s = %% %s\n", $blockCount, $blockSize,
#       $apparentSize->bstr(), $percentOccupied->bstr();
  return $percentOccupied->bstr();
}

##########################################################################

if ($argc < 2)
    {
    usage;
    exit 255;
    }

my $fileList = shift;
my @fileParts = split('\.', $fileList);
my $atimeListFile = "$tmpDir/trash.atime.$fileParts[scalar(@fileParts)-1]";
my %dirNameList;
my $dirCount = 0;
# 'root' is the top level directory /trash/ for files in that directory
$dirNameList{'root'} = 1;
++$dirCount;
while (my $analyzeDir = shift) {
   $dirNameList{$analyzeDir} = 1;
   ++$dirCount;
}

if ($dirCount < 1) {
    printf STDERR "ERROR: no directory list given after the file name list ?\n";
    usage;
    exit 255;
}

open (AT, ">$atimeListFile") or die "can not write to $atimeListFile";

printf STDERR "# %d directories specified\n", $dirCount;
printf STDERR "# reading file listing from: %s\n", $fileList;
printf STDERR "# writing access time stamps to: %s\n", $atimeListFile;

my %dirFileList;
my $dirFound = 0;
my $fileCount = 0;
open (FH, "<$fileList") or die "can not read file listing: '$fileList'";
while (my $file = <FH>) {
    chomp $file;
    ++$fileCount;
    my $dirName = $file;
    $dirName =~ s#/.*/trash/##;
    $dirName =~ s#/.*/trash.local/##;
    $dirName =~ s#/.*/trash.mount/##;
    if ($dirName =~ m#/#) {
	$dirName =~ s#/.*##;
	if (! exists($dirFileList{$dirName})) {
	    my @oneFileList;
	    push (@oneFileList, $file);
	    $dirFileList{$dirName} = \@oneFileList;
	    ++$dirFound;
	} else {
	    my $ref = $dirFileList{$dirName};
	    push (@$ref, $file);
	}
    } else {
	if (! exists($dirFileList{'root'})) {
	    my @oneFileList;
	    push (@oneFileList, $file);
	    $dirFileList{'root'} = \@oneFileList;
	    ++$dirFound;
	} else {
	    my $ref = $dirFileList{'root'};
	    push (@$ref, $file);
	}
    }
}
close (FH);

printf STDERR "# %d file count in %d directories, scanning sizes\n", $fileCount, $dirFound;

my $cumulativeBytes = Math::BigInt->bzero();
my $cumulativeBlocks = Math::BigInt->bzero();
# extReport accumulates text from the ct directory report to be
# output later
my $extReport = "";

printf "# kBytes  dir  blockCount  kBytesInBlocks  %%density\n";
printf "kBytes in subdirectories:\n";
foreach my $key (sort keys %dirNameList) {
    next if (!exists $dirFileList{$key});
    my $ref = $dirFileList{$key};
    my $fileCount = 0;
    my %fileCounts;
    my %fileSizes;
    my %fileBlocks;
    my $totalBlocks = Math::BigInt->bzero();
    my $totalBytes = Math::BigInt->bzero();

    for (my $i = 0; $i < scalar(@$ref); ++$i) {
	my $file = $ref->[$i];

	chomp($file);
	my @a = split('/',$file);
        my $pathElements = scalar(@a);
	# extract extension if there is one, otherwise file name
	my $ext = lc($a[$pathElements - 1]);
	if ($a[$pathElements-1] =~ m/\./) {
	    my @b = split('\.',$a[$pathElements-1]);
	    $ext = lc($b[scalar(@b)-1]);
	}
	# the stat returns nothing if the file does not exist
        my @s = stat($file);
# printf STDERR "#checking %s, statSize: %d\n", $file, scalar(@s);
	if (scalar(@s) > 10) {
	    ++$fileCount;
# my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
#     $atime,$mtime,$ctime,$blksize,$blocks) = stat($file);
	    my $fileSize = Math::BigInt->new($s[7]);
	    my $blocks = Math::BigInt->new($s[12]);
	    $fileCounts{$ext}++;
	    if (exists($fileSizes{$ext})) {
		$fileSizes{$ext}->badd($fileSize);
	    } else {
		$fileSizes{$ext} = Math::BigInt->new($s[7]);
	    }
	    $fileBlocks{$ext} += $s[12];
	    $totalBytes->badd($fileSize);
	    $totalBlocks->badd($blocks);
	    if ($blockSize < 1) { $blockSize = $s[11]; }
# printf STDERR "# %s\t%s\n", $fileSize->bstr(), $blocks->bstr();
	    printf AT "%d\t%s\n", $s[8], $file;
	}
    }
    if ($key =~ m/^root$|^ct$|^hgPcr$|^hgSs$|^hgc$|^udcCache$/) {
	$extReport .= sprintf("file counts, extension, sizes and blocks for %d files in /trash/%s\n", $fileCount, $key);
	foreach my $ext (sort keys %fileCounts)
	{
	    my $density =
		percentDensity($fileSizes{$ext}, $fileBlocks{$ext}, $blockSize);
	    my $realBytes = realSize($fileSizes{$ext}, $density);
	    $extReport .= sprintf("%d\t%s\t%s bytes %d blks %% %s dense -> %s actual bytes\n",
	    $fileCounts{$ext}, $ext, $fileSizes{$ext}->bstr(),
	    $fileBlocks{$ext}, $density, $realBytes);
	}
    }
    $cumulativeBytes->badd($totalBytes);
    $cumulativeBlocks->badd($totalBlocks);
    my $tBs = Math::BigInt->new($blockSize);
    $tBs->bmul($totalBlocks);
    my $percentDense = percentDensity($totalBytes, $totalBlocks, $blockSize);
    my $kBytes = Math::BigInt->new($totalBytes);
    $kBytes->bdiv($oneK);
    $tBs->bdiv($oneK);
    printf "%s\t%s\t%s\t%s\t%s\n", $kBytes->bstr(), $key, $totalBlocks->bstr(), $tBs->bstr(), $percentDense;
}

close (AT);

my $kBytes = Math::BigInt->new($cumulativeBytes);
$kBytes->bdiv($oneK);
my $percentDense = percentDensity($cumulativeBytes, $cumulativeBlocks, $blockSize);
my $tBs = Math::BigInt->new($blockSize);
$tBs->bmul($cumulativeBlocks);
$tBs->bdiv($oneK);
printf "%s\ttotal\t%s\t%s\t%s\n", $kBytes->bstr(), $cumulativeBlocks->bstr(), $tBs->bstr(), $percentDense;
printf "# file system block size: %d\n", $blockSize;

printf "file counts in sub-directories\n";
foreach my $key (sort keys %dirFileList) {
   my $ref = $dirFileList{$key};
   printf "%d\t%s\n", scalar(@$ref), $key;
}

if (length($extReport) > 0) {
   printf "%s", $extReport;
}
