#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit ~/kent/src/hg/utils/cloneEndParse.pl instead.

# $Id: cloneEndParse.pl,v 1.4 2007/08/03 21:42:07 hiram Exp $

# File: convertTxt
# Author: Heather Trumbower
# Based on /cluster/bin/scripts/convertBacEndPairInfo
# Date: May 2005
# Description: Converts *.txt files to 
# cloneEndsPair.txt and cloneEndsSingle.txt file used for creating 
# BAC End Pairs tracks
#	turned on warnings and strict, and annotated 2006-05-05 - Hiram

use warnings;
use strict;

########################################################################
# Usage message
sub usage()
{
    print STDERR
	"cloneEndParse - parse NCBI clone information into Pairs and Singles\n";
    print STDERR "usage: zcat all.txt.gz | ./cloneEndParse.pl /dev/stdin\n";
    print STDERR "or: ./cloneEndParse.pl all.txt\n";
    print STDERR
	"creates the files cloneEndPairs.txt and cloneEndSingles.txt\n";
    exit(255);
}

########################################################################
#  The isForward and isReverse are checking the End column of the
#  incoming data to make a judgement about strand.  However, For Hg18 I
#  see several other indicators in this column that are not checked
#  here:
#	DP6 F M13F M13R P6 R S S6 SP6 SP7 T T7 TJ TK
#	The DP6 P6 S6 and SP7 have only one instance each
#	and very few for M13R S T
#	big counts for: F M13F R SP6 T7 TJ TK
#  CORRECTION 2007-08-03 code TJ and TK reverse their meanings
#  CORRECTION 2007-08-03 code S and T reverse their meanings
########################################################################
sub isForward {
    my $end = shift(@_);
    if (($end =~ /^T7/) || ($end =~ /^M13F/) ||
	($end eq "F") || ($end =~ /^TK/) || ($end eq "S")) {
	return 1;
    } else {
	return 0;
    }
}

########################################################################
sub isReverse {
    my $end = shift(@_);
    if (($end =~ /^SP6/) || ($end eq "M13R") ||
	($end eq "R") || ($end eq "DP6") || ($end =~ /^TJ/) ||
	($end eq "P6") || ($end eq "S6") || ($end eq "SP7") || ($end eq "T")) {
	return 1;
    } else {
	return 0;
    }
}

########################################################################
my $argc = scalar(@ARGV);

if ($argc < 1)
{
    usage;
}

my $file = shift(@ARGV);
open(FILE, "<$file") or die "Could not open $file: $!";

my %found;	#	key is clone, value is 0 or 1
my %clone;	#	key is accession name, value is clone name
my %printa;	#	key is accession name, value is 0 or 1
my %print;	#	key is accession name, value is 0 or 1
my %end;	#	key is accession name, value is end name
my %t7;		#	key is clone name, value is list of accessions
my %sp6;	#	key is clone name, value is list of accessions

#	Parsing data input that looks like:
#  #Clone^I^Iacc.ver^IGI^I^ILibrary^ILength^IUniq Length^IVector^IEnd^IDate Analyzed$
#$
#  RP11-457I1^IAZ303307.1^I10300218^IRP11^I542^I453^IBAC^IT7^I2006-01-06$
#  RP11-457I1^IAZ303308.1^I10300220^IRP11^I526^I526^IBAC^ISP6^I2006-01-06$

# Read in and record end info
print STDERR "Reading in end info\n";
open (OUT, ">changes.txt");
while (my $line = <FILE>) {
  next if (substr($line,0,1) eq "#");
  chomp($line);
  my ($clone, $acc, $gi, $lib, $length, $uniqLength, $vector, $end, $date) = split('\t',$line);
  ($acc, my $ver) = split(/\./,$acc);
  $end =~ tr/a-z/A-Z/;

  $found{$clone} = 1;
  $clone{$acc} = $clone;
  $printa{$acc} = 0;
  $print{$clone} = 0;
  $end{$acc} = $end;
  if (&isForward($end)) {
    $t7{$clone} .= "$acc,";
  } elsif (&isReverse($end)) {
    $sp6{$clone} .= "$acc,";
  } elsif ($end) {
    print STDERR "End $end for $acc / $clone - unclassified\n";
  }
}
close(OUT);

my $pair = 0;

# Print out pairs
open(OUT, ">cloneEndPairs.txt");
print STDERR "Writing out pair info\n";
foreach my $clone (keys %found) {
  if ($t7{$clone} && $sp6{$clone}) {
    print OUT "$t7{$clone}\t$sp6{$clone}\t$clone\n";
    $print{$clone} = 1;
    my @acc = split(/\,/,$t7{$clone});
    for (my $i = 0; $i <= $#acc; $i++) {
       $printa{$acc[$i]} = 1;
    }
    @acc = split(/\,/,$sp6{$clone});
    for (my $i = 0; $i <= $#acc; $i++) {
       $printa{$acc[$i]} = 1;
    }
    $pair++;
  }
}
close(OUT);

my $single = 0;

# Print out singletons
print STDERR "Writing out singleton info\n";
open(OUT, ">cloneEndSingles.txt");
foreach my $acc (keys %printa) {
  my $clone = $clone{$acc};
  if (!$printa{$acc}) {
    if (&isForward($end{$acc})) {
      print OUT "$acc\t$clone\tT7\n";
    } elsif (&isReverse($end{$acc})) {
      print OUT "$acc\t$clone\tSP6\n";
    } else {
      print OUT "$acc\t$clone\tUNK\n";
    }
    $single++;
  }
}
close(OUT);
print STDERR "$pair pairs and $single singles\n";

