#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1) {
  printf STDERR "usage: ./scanOne.pl GCF_00123vN > geneDesc.GCF_00123vN\n";
  exit 255;
}

my %productToGene;	# key is protein name, value is gene name
my %dupList;	# key is one gene name, value is other for same protein

open (FH, "hgsql -N -e 'select name,name2 from ncbiGene;' staAur2|") or die "can not hgsql staAur2.ncbiGene table";
while (my $line = <FH>) {
  chomp $line;
  my ($geneName, $protein) = split('\s+', $line);
  if (defined($productToGene{$protein})) {
    if ($geneName ne $productToGene{$protein}) {
      printf STDERR "WARNING: duplicate names not matching $geneName != $productToGene{$protein} for $protein\n";
      if (defined($dupList{$protein})) {
        die "ERROR: second duplicate $geneName for $protein $dupList{$protein}";
      } else {
         $dupList{$protein} = $geneName;
      }
    }
  } else {
    $productToGene{$protein} = $geneName;
  }
}
close (FH);

my $gcf = shift;
my $seqDir = "/hive/data/genomes/staAur2/sequences/$gcf";
my $protFaa = `ls $seqDir/*_protein.faa.gz`;
chomp $protFaa;
if (! -s "$protFaa" ) {
  die "can not find protein.faa: '$protFaa'\n";
}
open (FH, "zgrep '^>' $protFaa|") or die "can not zgrep $protFaa";
while (my $line = <FH>) {
  chomp $line;
  my ($protein, $html) = split('\s+', $line, 2);
  $protein =~ s/^>//;
  my $geneSymbol = "";
  if (defined($productToGene{$protein})) {
     $geneSymbol = $productToGene{$protein};
  } else {
     if (defined($dupList{$protein})) {
       $geneSymbol = $dupList{$protein};
     } else {
       die "ERROR: can not find $protein on dupList";
     }
  }
  printf "%s\t<h3>Gene Description</h3>%s\n", $geneSymbol, $html;
  if (defined($dupList{$protein})) {
       $geneSymbol = $dupList{$protein};
       printf "%s\t<h3>Gene Description</h3>%s\n", $geneSymbol, $html;
  }
}
close (FH);

__END__

+-------+----------------+------+-----+---------+-------+
| Field | Type           | Null | Key | Default | Extra |
+-------+----------------+------+-----+---------+-------+
| name  | varchar(255)   | YES  |     | NULL    |       |
| html  | varchar(30000) | YES  |     | NULL    |       |
+-------+----------------+------+-----+---------+-------+

