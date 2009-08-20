#!/usr/bin/env perl
# File: fixPrimersQueryGaps
# Author: Terry Furey
# Date: 7/5/2003
# Project: Human
# Description: Fix query gap sizes from output of isPcr to match 
# true pcr product size

use strict;
use warnings;

# Usage message
if ($#ARGV < 0) {
  print STDERR "USAGE: fixPrimersQueryGaps <primer file> <psl file>\n";
  exit(1);
}

# Read in and check parameters
my $file = shift(@ARGV);
open(FILE, "<$file") || die("Can not open primer file '$file'");
my $psl = shift(@ARGV);
open(PSL, "$psl") || die("Can not open psl file '$psl'");

my %length;

printf STDERR "reading pcr product lengths from $file\n";
# Read in lengths of pcr products
while(my $line = <FILE>) {
  chomp($line);
  my ($id, $left, $right, $length, $otherid) = split('\s+',$line);
  if ($length eq "-") {
      $length{$id} = 500;
  } elsif ($length =~ m/-/) {
      my ($start, $end) = split('-',$length);
      $length{$id} = int(($end - $start)/2) + $start;
  } else {
      $length{$id} = $length;
  }
  my @left = split(//,$left);
  my @right = split(//,$right);
  my $numN = $length{$id} - $#left - $#right - 2;
  if ($numN <=0) {
      $length{$id} = 50;
  }
}

# Correct query gap sizes in psl file
printf STDERR "reading psl file $psl\n";
while (my $line = <PSL>) {
    chomp($line);
    my @fields = split("\t",$line);
    if ($fields[4] != 1) {
	print STDERR "check $line\n";
    }
    my ($dbsts, $id) = split("_", $fields[9]);
    # change query gap
    die "can not find length for id $id" if (!exists($length{$id}));
    $fields[5] = $length{$id} - $fields[0] - $fields[1];
    # change query size
    $fields[10] = $length{$id};
    $fields[12] = $length{$id};
    # change query starts
    my @qstarts = split(/\,/, $fields[19]);    
    my @blocks = split(/\,/, $fields[18]);
    my $secondStart = $length{$id} - $blocks[1];
    $fields[19] = "$qstarts[0],$secondStart,";
    # put back together and print
    $line = join("\t", @fields);
    print "$line\n";
}
