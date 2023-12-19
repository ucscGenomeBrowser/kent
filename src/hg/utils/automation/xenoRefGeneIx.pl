#!/usr/bin/env perl

# construct ix.txt index from a xenoRefGene bigGenePred file
# index key is the name of the gene from column 4 of the bigGenePred
# alias names for that are constructed:
#   the name without any .nnn suffix
#   name2 if present, and name2 without .nnn suffix
#   geneName if present, and geneName without .nnn suffix
#   and geneName2 to allow searching the species of origin for the mRNA

# table bigGenePred
# "bigGenePred gene models"
#    (
#    string chrom;       "Reference sequence chromosome or scaffold"
#    uint   chromStart;  "Start position in chromosome"
#    uint   chromEnd;    "End position in chromosome"
# 4 #    string name;        "Name or ID of item, ideally both human readable and unique"
#    uint score;         "Score (0-1000)"
#    char[1] strand;     "+ or - for strand"
#    uint thickStart;    "Start of where display should be thick (start codon)"
#    uint thickEnd;      "End of where display should be thick (stop codon)"
#    uint reserved;       "RGB value (use R,G,B string in input file)"
#    int blockCount;     "Number of blocks"
#    int[blockCount] blockSizes; "Comma separated list of block sizes"
#    int[blockCount] chromStarts; "Start positions relative to chromStart"
# 13 #    string name2;       "Alternative/human readable name"
#    string cdsStartStat; "Status of CDS start annotation (none, unknown, incomplete, or complete)"
#    string cdsEndStat;   "Status of CDS end annotation (none, unknown, incomplete, or complete)"
#    int[blockCount] exonFrames; "Exon frame {0,1,2}, or -1 if no frame for exon"
#    string type;        "Transcript type"
# 18 #    string geneName;    "Primary identifier for gene"
# 19 #    string geneName2;   "species of origin of the mRNA"
#    string geneType;    "Gene type"
#    )


use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1 ) {
  printf STDERR "usage: xenoRefGeneIx.pl <bpgInput.txt> | sort -u > ix.txt\n";
  printf STDERR "then run ixIxx on ix.txt:\n";
  printf STDERR " ixIxx ix.txt out.ix out.ixx\n";
  exit 255;
}

# some of the Arabidopsis thaliana genes are coming in as pairs
# of different names separated by '; ', process both names

sub processOne($$) {
  my ($nameHash, $someName) = @_;
  if ($someName =~ m/; /) {
    my ($name1, $name2) = split('; ', $someName);
    $nameHash->{lc($name1)} += 1;
    $nameHash->{lc($name2)} += 1;
    my $noSuffix = $name1;
    $noSuffix =~ s/\.[0-9][0-9]*$//;
    $nameHash->{lc($noSuffix)} += 1;
    $noSuffix = $name2;
    $noSuffix =~ s/\.[0-9][0-9]*$//;
    $nameHash->{lc($noSuffix)} += 1;
  } elsif ($someName =~ m/\s/) {
    $nameHash->{lc($someName)} += 1;
    my @names = split('\s+', $someName);
    foreach my $name (@names) {
      $nameHash->{lc($name)} += 1;
      my $noSuffix = $name;
      $noSuffix =~ s/\.[0-9][0-9]*$//;
      $nameHash->{lc($noSuffix)} += 1;
    }
  } else {
    $nameHash->{lc($someName)} += 1;
    my $noSuffix = $someName;
    $noSuffix =~ s/\.[0-9][0-9]*$//;
    $nameHash->{lc($noSuffix)} += 1;
  }
}

my $gpFile = shift;

if ($gpFile =~ m/.gz$/) {
  open (FH, "zcat $gpFile|") or die "ERROR: xenoRefGeneIx.pl can not read '$gpFile'";
} else {
  open (FH, "<$gpFile") or die "ERROR: xenoRefGeneIx.pl can not read '$gpFile'";
}
while (my $line = <FH>) {
  next if ($line =~ m/^#/);
  chomp ($line);
  my ($chrom, $chromStart, $chromEnd, $name, $score, $strand, $thickStart, $thickEnd, $reserved, $blockCount, $blockSizes, $chromStarts, $name2, $cdsStartStat, $cdsEndStat, $exonFrames, $type, $geneName, $geneName2, $geneType) = split('\t', $line);
  my %allNames;	# key is name, value is count of times seen
  processOne(\%allNames, $name) if (length($name));
  processOne(\%allNames, $name2) if (length($name2));
  processOne(\%allNames, $geneName) if (length($geneName));
  processOne(\%allNames, $geneName2) if (length($geneName2));
  my $howMany = 0;
  foreach my $someName (keys %allNames) {
     $howMany += 1;
  }
  if ($howMany > 1) {
    printf "%s", $name;
    foreach my $someName (sort keys %allNames) {
      next if ($someName =~ m/\Q$name\E/i);
      printf "\t%s", $someName;
    }
    printf "\n";
  }
}
close (FH);
