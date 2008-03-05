#!/usr/bin/env perl
#
#	fixStepToBedGraph.pl - read fixedStep wiggle input data,
#		output four column bedGraph format data
#
#	used thusly:
#	zcat fixedStepData.gz | ./fixStepToBedGraph.pl \
#		| gzip > bedGraph.gz

#	$Id: fixStepToBedGraph.pl,v 1.3 2008/03/05 18:42:01 kate Exp $

use warnings;
use strict;

my $position = 0;
my $chr = "";
my $step = 1;
my $span = 1;

print STDERR "usage: fixStepToBedGraph.pl\n";
print STDERR "\trun in a pipeline like this:\n";
print STDERR "usage: ",
"zcat fixedStepData.gz | fixStepToBedGraph.pl | gzip > bedGraph.gz",
	"\n";
print STDERR "reading input data from stdin ...\n";

while (my $dataValue = <>)
{
chomp $dataValue;
if ( $dataValue =~ m/^fixedStep / ) {
    $position = 0;
    $chr = "";
    $step = 1;
    $span = 1;
    my @a = split('\s', $dataValue);
    for (my $i = 1; $i < scalar(@a); ++$i) {
	    my ($ident, $value) = split('=',$a[$i]);
	    if ($ident =~ m/chrom/) { $chr = $value; }
	    elsif ($ident =~ m/start/) { $position = $value-1; }
	    elsif ($ident =~ m/step/) { $step = $value; }
	    elsif ($ident =~ m/span/) { $span = $value; }
	    else {
	    print STDERR "ERROR: unrecognized fixedStep line: $dataValue\n";
		exit 255;
	    }
    }
  } else {
      printf "%s\t%d\t%d\t%g\n", $chr, $position, $position+$span, $dataValue;
      $position = $position + $step;
  }
}
