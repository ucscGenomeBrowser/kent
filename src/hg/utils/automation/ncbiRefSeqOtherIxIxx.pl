#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 2) {
  printf STDERR "usage: ncbiRefSeqOtherIxIxx.pl ncbiRefSeqOther.as db.other.extras.bed \\\n\t> ncbiRefSeqOther.ix.tab\n";
  printf STDERR "uses ncbiRefSeqOther.as as a guide of what columns to index\n\tin the extras.bed file\n";
  printf STDERR "use ixIxx on the output file\n";
  exit 255;
}

my $asFile = shift;
my $extrasBed = shift;

my @columns;
my $nameOffset = 0;
# determine which columns to index from the .as file
open (FH, "cat -b $asFile|") or die "can not read $asFile";
while (my $line = <FH>) {
  chomp $line;
  $line =~ s/^ +//;
  if ($nameOffset > 0) {
    if ($line =~ m/string gene;/) {
      my ($lineNo, undef) = split('\s+', $line, 2);
      push @columns, $lineNo-$nameOffset
    } elsif ($line =~ m/string GeneID;/) {
      my ($lineNo, undef) = split('\s+', $line, 2);
      push @columns, $lineNo-$nameOffset
    } elsif ($line =~ m/string WormBase;/) {
      my ($lineNo, undef) = split('\s+', $line, 2);
      push @columns, $lineNo-$nameOffset
    } elsif ($line =~ m/string MIM;/) {
      my ($lineNo, undef) = split('\s+', $line, 2);
      push @columns, $lineNo-$nameOffset
    } elsif ($line =~ m/string MGI;/) {
      my ($lineNo, undef) = split('\s+', $line, 2);
      push @columns, $lineNo-$nameOffset
    } elsif ($line =~ m/string miRBase;/) {
      my ($lineNo, undef) = split('\s+', $line, 2);
      push @columns, $lineNo-$nameOffset
    } elsif ($line =~ m/string product;/) {
      my ($lineNo, undef) = split('\s+', $line, 2);
      push @columns, $lineNo-$nameOffset
    } elsif ($line =~ m/string geneSynonym;/) {
      my ($lineNo, undef) = split('\s+', $line, 2);
      push @columns, $lineNo-$nameOffset
    } elsif ($line =~ m/string ID;/) {
      my ($lineNo, undef) = split('\s+', $line, 2);
      push @columns, $lineNo-$nameOffset
    }
  } elsif ($line =~ m/string name;/) {
    my ($lineNo, undef) = split('\s+', $line, 2);
    $nameOffset = $lineNo - 4;  # name is column 4 in the bed file
    push @columns, $lineNo-$nameOffset
  }
}
close (FH);

my @orderCut = sort { $a <=> $b } @columns;
my $cutString = join ',', @orderCut;

printf STDERR "# cut -f $cutString $extrasBed\n";

if ($extrasBed =~ m/.gz$/) {
   open (FH, "zcat $extrasBed | cut -f $cutString|") or die "can not zcat $extrasBed";
} else {
   open (FH, "cut -f $cutString $extrasBed |") or die "can not read $extrasBed";
}

my %itemsOut;	# key is item name, value is hash with all other names as keys
while (my $line = <FH>) {
  chomp $line;
  $line =~ s/,/\t/g;
#  printf STDERR "# $line\n";
  my ($itemName, $otherNames) = split('\s+', $line, 2);
  if (! exists($itemsOut{$itemName})) {
     my %othersHash;
     $itemsOut{$itemName} = \%othersHash;
  }
  my $itemPtr = $itemsOut{$itemName};
  my @others = split('\s+', $otherNames);
  for (my $i = 0; $i < scalar(@others); ++$i) {
     $others[$i] =~ s/^MGI://;
     $others[$i] =~ s/^Em://;
     $itemPtr->{$others[$i]} = 1;
     my $noSuffix=$others[$i];
     $noSuffix =~ s/\.[0-9]+$//;
     $itemPtr->{$noSuffix} = 1;
  }
}
close (FH);

foreach my $itemName (sort keys %itemsOut) {
  my $itemPtr = $itemsOut{$itemName};
  my $othersOut = "";
  foreach my $otherName (sort keys %$itemPtr) {
     if ($otherName ne $itemName && length($otherName) > 1) {
        $othersOut .= " " . $otherName;
     }
  }
  printf "%s%s\n", $itemName, $othersOut if (length($othersOut) > 1)
}
