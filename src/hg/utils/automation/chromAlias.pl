#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;

my $argc = scalar(@ARGV);
if ($argc < 1) {
  printf STDERR "usage: chromAlias.pl <ucsc.refseq.tab> <ucsc.genbank.tab> \\\n\t<ucsc.ensembl.tab> <ucsc.others.tab> > <db>.chromAlias.tab\n";
  printf STDERR "must have at least one of these input files, others when available\n";
  printf STDERR "the names of the input files must be of this pattern so\n";
  printf STDERR "the name of the alias can be identified\n";
  exit 255;
}

my %names;  # key is name identifier (refseq, genbank, ensembl, flybase, etc...)
		#  value is a hash with key identifer name, value ucsc chr name
my %chrNames;	# key is UCSC chrom name, value is number of times seen

while (my $file = shift @ARGV) {
  my $name = $file;
  $name =~ s/ucsc.//;
  $name =~ s/.tab//;
  printf STDERR "# working: %s\n", $name;
  my $namePtr;
  if (exists($names{$name})) {
     $namePtr = $names{$name};
  } else {
     my %nameHash;
     $namePtr = \%nameHash;
     $names{$name} = $namePtr;
  }
  open (FH, "<$file") or die "can not read $file";
  while (my $line = <FH>) {
     chomp $line;
     my ($chr, $other) = split('\t+', $line);
     if (exists($namePtr->{$chr})) {
       printf STDERR "# warning, identical UCSC chrom $chr in $name for $other\n";
       $namePtr->{$chr} = sprintf("%s\t%s", $namePtr->{$chr}, $other);
     } else {
       $namePtr->{$chr} = $other;
     }
     $chrNames{$chr} += 1;
   }
   close (FH);
}

foreach my $chr (sort keys %chrNames) {
  my %outNames;	# key is other identifier, value is csv list of sources
  foreach my $name (sort keys %names) {
    my $namePtr = $names{$name};
    if (exists($namePtr->{$chr})) {
      my $otherId = $namePtr->{$chr};
      if (! $otherId) {
        die "namePtr->chr exists but is |$otherId| for chr |$chr| (tab-sep?)";
      }
      my @a;
      if ($otherId =~ m/\t/) {
	  @a = split('\t', $otherId);
      } else {
	  $a[0] = $otherId;
      }
      for (my $i = 0; $i < scalar(@a); ++$i) {
         if (exists($outNames{$a[$i]})) {
	    $outNames{$a[$i]} = sprintf("%s,%s", $outNames{$a[$i]}, $name);
         } else {
	    $outNames{$a[$i]} = $name;
         }
      }
    }
  }
  foreach my $otherName (sort keys %outNames) {
	printf "%s\t%s\t%s\n", $otherName, $chr, $outNames{$otherName};
  }
}
