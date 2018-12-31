#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

sub usage() {
    printf STDERR "usage: asciiTree.pl <file.nh>\n";
    printf STDERR "will print out the tree with one species per line\n";
    printf STDERR "conveniently indented to imitate a graphical image.\n";
    exit 255;
}

sub fixSciNotation($) {
  my ($string) = @_;
  my @branches = split(':', $string);
  my $answer = "";
  if (3 == scalar(@branches)) {
     $answer = "$branches[0]";
     my $dist = $branches[1];
     $dist =~ s/\)//g;
     $answer = sprintf("%s:%.8f", $answer, $dist);
     $answer =~ s/0+$//;
     $dist = $branches[2];
     $dist =~ s/\).*//;
     $answer = sprintf("%s):%.8f", $answer, $dist);
     $answer =~ s/0+$//;
  } elsif (2 == scalar(@branches)) {
     $answer = "$branches[0]";
     my $dist = $branches[1];
     $dist =~ s/\);//;
     $answer = sprintf("%s:%.8f", $answer, $dist);
     $answer =~ s/0+$//;
  } else {
     $answer = $string;
  }
  return $answer;
}

if ($argc != 1) {
   usage;
}

my $file = shift;

my $nhString = "";

open (FH, "<$file") or die "can not read $file";
while (my $line = <FH>) {
  $nhString .= $line;
}
close (FH);

$nhString =~ s/\s//g;
my @species = split(',', $nhString);
my $indent = 0;
for (my $i = 0; $i < scalar(@species)-1; ++$i) {
  for (my $j = 0; $j < $indent; ++$j) {
    print " ";
  }
  my $thisLine = $species[$i];
  my $j = 0;
  if (substr($thisLine,$j++,1) ne "(") {
    $indent -= 1;
  } else {
    while (substr($thisLine,$j++,1) eq "(") {
      ++$indent;
    }
  }
  my $fixedSciNotation = fixSciNotation($species[$i]);
#  printf "%s,\n", $species[$i];
  printf "%s,\n", $fixedSciNotation;
}
my $fixedSciNotation = fixSciNotation($species[-1]);
#printf "%s\n", $species[-1];
printf "%s);\n", $fixedSciNotation;

__END__

    KC545391v1:1.01393e-05):9.84274e-06,
   KC545392v1:9.59399e-06):0.0184195,
  JN638998v1:0.00984427):9.54171,
 NC_024781v1:0.302263):0.121363,
NC_001608v3:0.121363);

