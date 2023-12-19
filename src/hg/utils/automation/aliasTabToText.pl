#!/usr/bin/env perl

# aliasTabToText.pl - convert the database version chromAlias table
#                   - to the hub text version chromAlias.txt file.

use strict;
use warnings;

my %nameLabel;	# key is source name, value is a hash where
                # key is assembly chrom name, value is this source name

my %chrNames;	# key is chrom name, value is 1 for existence check

my $argc = scalar(@ARGV);
if ($argc != 1) {
  printf STDERR "usage: aliasTabToText.pl asmId.chromAlias.tab > asmId.chromAlias.txt\n";
  printf STDERR "where the asmId.chromAlias.tab file is a dump of the chromAlias table:\n";
  printf STDERR "  hgsql -N -e 'select * from chromAlias;' asmId > asmId.chromAlias.tab\n";
  printf STDERR "This is a conversion of the database chromAlias table to the\n";
  printf STDERR "text file equivalent for use in an assembly hub.\n";
  exit 255;
}

my $tabFile = shift;

open (FH, "<$tabFile") or die "can not read $tabFile";
while (my $line = <FH>) {
  chomp $line;
  my ($sourceName, $chrName, $sourceList) = split('\t', $line);
  my @sources = split(',', $sourceList);
  foreach my $source (@sources) {
     if (!defined($nameLabel{$source})) {
       my %h;
       $nameLabel{$source} = \%h;
     }
     my $srcPtr = $nameLabel{$source};
     $srcPtr->{$chrName} = $sourceName;
  }
  $chrNames{$chrName} = 1;
}
close (FH);

printf "# ucsc";
my @sourceList;
foreach my $source (sort keys %nameLabel) {
   push @sourceList, $source;
   printf "\t%s", $source;
}
printf "\n";

foreach my $chrName (sort keys %chrNames) {
  printf "%s", $chrName;
  foreach my $source (@sourceList) {
    my $srcPtr = $nameLabel{$source};
    if (defined($srcPtr->{$chrName})) {
      printf "\t%s", $srcPtr->{$chrName};;
    } else {
      printf "\t";
    }
  }
  printf "\n";
}
__END__

  head *.tab
1       chr1    assembly,ensembl
CM000663.2      chr1    genbank
NC_000001.11    chr1    refseq

