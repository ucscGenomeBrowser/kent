#!/usr/bin/env perl
#
#  DO NOT EDIT the /cluster/bin/scripts/ copy -- source is in CVS:
#  kent/src/hg/makeDb/hgLoadWiggle/varStepToBedGraph.pl
#
#	varStepToBedGraph.pl - take variableStep wiggle data (span=1 only)
#		and convert it to bedGraph bed-like format.

#	$Id: varStepToBedGraph.pl,v 1.2 2005/06/01 16:47:24 angie Exp $

use warnings;
use strict;

sub usage() {
  my ($status) = @_;
  my $bn=`basename $0`;
  chomp $bn;
  print STDERR "usage: $bn <variable step format file names | stdin>\n";
  exit $status;
}
my $argc = scalar(@ARGV);	#	arg count

&usage(255) if ($argc < 1);

my $linesRead = 0;
my $dataLines = 0;
my $variableDeclarations = 0;
my $chrom;
my $span=1;

shift @ARGV if ($ARGV[0] eq "stdin");

while (my $line=<>) {
  ++$linesRead;
  next if ($line =~ m/^\s*#/);
  next if ($line =~ m/^(track|browser)\s+/);
  chomp $line;
  if ($line =~ s/^variableStep\s+//) {
    $span=1;
    (my @a) = split('\s+',$line);
    for (my $i = 0; $i < scalar(@a); ++$i) {
      if ($a[$i] =~ m/^chrom=/) {
	(undef, $chrom) = split('=',$a[$i]);
      } elsif ($a[$i] =~ m/^span=/) {
	(undef, $span) = split('=',$a[$i]);
      } else {
	die "ERROR: illegal field $a[$i] in variableStep declaration " .
	  "at line # $.\n'$line'\n";
      }
    }
    ++$variableDeclarations;
  } else {
    die "data line before variableStep line (line $.)\n" if (! defined $chrom);
    my ($position, $value) = split('\s+',$line);
    ++$dataLines;
    printf "%s\t%d\t%d\t%s\n", $chrom, $position-1, $position-1+$span, $value;
  }
}

print STDERR "Processed $linesRead lines input, $dataLines data lines, " .
  "$variableDeclarations variable step declarations\n";
