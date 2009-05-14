#!/usr/local/bin/perl
# File: fixPrimersQueryGaps
# Author: Terry Furey
# Date: 7/5/2003
# Project: Human
# Description: Fix query gap sizes from output of isPcr to match 
# true pcr product size

# Usage message
if ($#ARGV < 0) {
  print stderr "USAGE: fixPrimersQueryGaps <primer file> <psl file>\n";
  exit(1);
}

# Read in and check parameters
$file = shift(@ARGV);
open(FILE, "<$file") || die("Could not open $file\n");
$psl = shift(@ARGV);
open(PSL, "$psl") || die("Could not open $psl\n");

# Read in lengths of pcr products
while($line = <FILE>) {
  chomp($line);
  ($id, $left, $right, $length, $otherid) = split(' ',$line);
  if ($length == "-") {
      $length{$id} = 500;
  } elsif ($length =~ "-") {
      ($start, $end) = split("-",$length);
      $length{$id} = int(($end - $start)/2) + $start;
  } else {
      $length{$id} = $length;
  }
  @left = split(//,$left);
  @right = split(//,$right);
  $numN = $length - $#left - $#right - 2;
  if ($numN <=0) {
      $length{$id} = 50;
  }
}

# Correct query gap sizes in psl file
while ($line = <PSL>) {
    chomp($line);
    @fields = split("\t",$line);
    if ($fields[4] != 1) {
	print stderr "check $line\n";
    }
    ($dbsts, $id) = split("_", $fields[9]);
    # change query gap
    $fields[5] = $length{$id} - $fields[0] - $fields[1];
    # change query size
    $fields[10] = $length{$id};
    $fields[12] = $length{$id};
    # change query starts
    @qstarts = split(/\,/, $fields[19]);    
    @blocks = split(/\,/, $fields[18]);
    $secondStart = $length{$id} - $blocks[1];
    $fields[19] = "$qstarts[0],$secondStart";
    # put back together and print
    $line = join("\t", @fields);
    print "$line\n";
}
