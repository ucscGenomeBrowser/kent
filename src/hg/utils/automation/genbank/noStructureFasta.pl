#!/usr/bin/env perl

use strict;
use warnings;

sub usage() {
  printf STDERR "usage: ./noStructureFasta.pl [ucsc|ncbi] <pathTo>/*noStructure.agp.gz <pathTo>/*_genomic.fna.gz\n";
  printf STDERR "output FASTA is to stdout, redirect to > noStructure.fa\n";
  printf STDERR "the ucsc|ncbi selects the naming scheme\n";
}

my $argc = scalar(@ARGV);

if ($argc < 3) {
  usage;
  exit 255;
}

my $ncbiUcsc = shift(@ARGV);
my $agpFile = shift(@ARGV);
my $fnaFile = shift(@ARGV);

my %ncbiToUcsc;   # key is NCBI accession, value is UCSC name

open (FH, "zcat $agpFile|") or die "can not read $agpFile";
while (my $line = <FH>) {
  chomp $line;
  my ($ucscName, undef, undef, undef, undef, $ncbiName, undef) = split('\s+', $line, 7);
  $ncbiToUcsc{$ncbiName} = $ucscName;
}
close (FH);

open (FH, "zcat $fnaFile|") or die "can not read $fnaFile";
while (my $line = <FH>) {
  if ($line =~ m/^>/) {
    chomp $line;
    $line =~ s/^>//;
    $line =~ s/ .*//;
    die "can not find $line in $fnaFile" if (! exists($ncbiToUcsc{$line}));
    if ($ncbiUcsc eq "ncbi") {
      printf ">%s\n", $line;
    } else {
      printf ">%s\n", $ncbiToUcsc{$line};
    }
  } else {
    print $line;
  }
}
close (FH);
