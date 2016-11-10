#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc != 1) {
  printf STDERR "usage: extractSource.pl <allGenes.gtf.gz> | sort -u > ensemblSource.tab\n";
  exit 255;
}

my $gtfFile = shift;

if ( $gtfFile =~ m/.*.gz/) {
  open (FH, "zcat $gtfFile|") or die "can not zcat $gtfFile";
} else {
  open (FH, "<$gtfFile") or die "can not read $gtfFile";
}
while (my $line = <FH>) {
  next if ($line =~ m/^#/);
  chomp $line;
  my @a = split('\t', $line);
  die "fields not nine at '$line'" if (scalar(@a) != 9);
  my @b = split(';', $a[8]);
  my %tags;
  for (my $i = 0; $i < scalar(@b); ++$i) {
    my $tagValue = $b[$i];
    my $tag = $tagValue;
    $tag =~ s/^\s+//;
    $tag =~ s/\s.*//;
    $tagValue =~ s/\s*$//;
    $tagValue =~ s/^\s+//;
    $tagValue =~ s/[^\s]*\s?//;
    $tagValue =~ s/"//g;
    $tags{$tag} = $tagValue;
# printf STDERR "'%s' = '%s'\n", $tag, $tagValue;
  }
  if (exists($tags{"transcript_id"}) && exists($tags{"gene_biotype"})) {
    my $version="";
    $version="." . $tags{"transcript_version"} if exists($tags{"transcript_version"});
    printf "%s%s\t%s\n", $tags{"transcript_id"}, $version, $tags{"gene_biotype"};
  }
}
close (FH);
