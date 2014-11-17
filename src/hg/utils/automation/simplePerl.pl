#!/usr/bin/env perl
#
# example perl code to demonstrate common operations
# reads a wigFix wiggle ascii file, splits it into per-chrom named files
#

use strict;
use warnings;

sub usage() {
  printf STDERR "usage: ./simplePerl.pl <argument> [more arguments]\n";
  printf STDERR " will process arguments as files to read\n";
}

my $debug = 0;

# for hash variables:

my %hashVar;   # where key is some string, value is some contents as:
               #   $hashVar{$someKey} = $someValue
my $hashVar;   # same name for some other variable, probably bad programming
my @hashVar;   # same name for an array, probably bad programming practice

# for array variables:

my @arrayVar;  # array indexes are integers 0 to N
               #   $arrayVar[$index] = $someValue
my $arrayVar;  # same name for some other variable

# check command line argument count
my $argc = scalar(@ARGV);

if ($argc < 1) {
  usage;
  exit 255;
}

my $fileOut = "";
mkdir "splitResult";

while (my $file = shift @ARGV) {
  printf STDERR "# working on: %s\n", $file;
  if ($file =~ m/.gz$/) {   # can manage gzip files also
    open (FH, "zcat $file|") or die "can not zcat $file";
  } else {
    open (FH, "<$file") or die "can not open $file";
  }
  while (my $line = <FH>) {
    chomp $line;   # remove the end of line character, whatever it may be
    printf STDERR "# '%s'\n", $line if ($debug > 0);;
    if ($line =~ m/^fixedStep/) {
      my @a = split('\s+', $line);
      my $chrName = "";
      for (my $i = 0; $i < scalar(@a); ++$i) {
        if ($a[$i] =~ m/^chrom/) {
          (undef, $chrName) = split('=', $a[$i]);
          last;
        }
      }
      close(FO) if (length($fileOut) > 0);
      $fileOut = "splitResult/${chrName}.wigFix";
      open (FO, ">>$fileOut") or die "can not append to $fileOut";
      printf FO "%s\n", $line;
    } else {
      die "found lines before first fixedStep declaration"
           if (length($fileOut) < 1);
      printf FO "%s\n", $line
    }
  }
  close (FH);
}
