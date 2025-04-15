#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc != 2) {
  printf STDERR "usage: updateName2.pl <file.attrs.txt> <file.gp>\n";
  printf STDERR "Will read the file.gp converting it to a bigGenePred\n";
  printf STDERR "    with column 18 of the bigGenePred outputting the\n";
  printf STDERR "    GeneID identifier when found in the file.attrs.txt\n";
  printf STDERR "Also fills in 'type' and 'geneType' columns when available. \n";
  printf STDERR "Output of the revised bigBigGenePred will be to stdout.\n";
  printf STDERR "This function is used in the doNcbiRefSeq.pl script.\n";
  exit 255;
}
my $attrs = shift;
my $gpFile = shift;

my %parent;	# key is gene name, value is parent name
my %dupList;	# key is gene name, placed on this list when duplicate IDs found
		# for the same named gene, avoid these.
my %geneId;	# key is gene name from colunn 1, value is GeneID number
my %bioType;	# key is gene name from column 1, value is
		#    the string associated with gene_biotype when found
my %Type;	# key is gene name from column 1, value is
		#    the string associated with Type when found
my %descr;	# key is gene name, value is description string

open (my $fh, "-|", "egrep -w 'Parent|GeneID|Type|gene_biotype|description' $attrs") or die "can not grep $attrs";
while (my $line = <$fh>) {
  chomp $line;
  my @a = split(/\s+/, $line, 3);
  my $geneName = $a[0];
  next if (defined($dupList{$geneName}));
  if ($a[1] =~ /Dbxref/i) {
      my $geneId = $a[-1];
      my @b = split(/,/, $geneId);
  # could be csv list of gene IDs here, looking for the 'GeneID'
      for my $maybeGeneId (@b) {
         if ($maybeGeneId =~ m/^GeneID/i) {
             $maybeGeneId =~ s/GeneID://i;
             if (defined($geneId{$geneName})) {
                 if ($geneId{$geneName} ne $maybeGeneId) {
                    $dupList{$geneName} = "$geneId{$geneName},${maybeGeneId}";
                    delete $geneId{$geneName};
                    next;
                 }
             } else {
                 $geneId{$geneName} = $maybeGeneId;
             }
             last;
         }
      }
  } elsif ($a[1] =~ /description/i) {
      if (defined($descr{$geneName})) {
         if ($a[2] ne $descr{$geneName}) {
            $dupList{$geneName} = "$geneName,$descr{$geneName}.$a[2]";
            delete $descr{$geneName};
         }
      } else {
         $descr{$geneName} = $a[2];
      }
  } elsif ($a[1] =~ /Parent/i) {
      if (defined($parent{$geneName})) {
         if ($a[2] ne $parent{$geneName}) {
            $dupList{$geneName} = "$geneName,$parent{$geneName}.$a[2]";
            delete $parent{$geneName};
         }
      } else {
         $parent{$geneName} = $a[2];
      }

  } elsif ($a[1] =~ /gene_biotype/i) {
      if (defined($bioType{$geneName})) {
         if ($a[2] ne $bioType{$geneName}) {
            $dupList{$geneName} = "$geneName,$bioType{$geneName}.$a[2]";
            delete $bioType{$geneName};
         }
      } else {
         $bioType{$geneName} = $a[2];
      }
  } elsif ($a[1] =~ /^Type$/i) {
      if (defined($Type{$geneName})) {
         if ($a[2] ne $Type{$geneName}) {
            $dupList{$geneName} = "$geneName,$Type{$geneName}.$a[2]";
            delete $Type{$geneName};
         }
      } else {
         $Type{$geneName} = $a[2];
      }
  }
}
close ($fh);

my $updatedNames = 0;
my $totalItems = 0;
open ($fh, "-|", "genePredToBigGenePred $gpFile stdout") or die "can not read $gpFile";
while (my $line = <$fh>) {
  chomp $line;
  ++$totalItems;
  # the -1 keeps the trailing empty field at the end of the line
  my @a = split(/\t/, $line, -1);
  my $sizeA = scalar(@a);
  # if name is equal to geneName see if geneName can be improved
  if ($a[3] eq $a[17]) {
     if (defined($geneId{$a[3]})) {
        $a[17] = $geneId{$a[3]};
        ++$updatedNames;
        # if name2 is equal to geneName2 reproduce name in geneName2
        if ($a[12] eq $a[18]) {
             $a[18] = $a[3];
        }
     }
  }
  #  "Transcript type"
  if (length($a[16]) < 1) {
     if (defined($bioType{$a[3]})) {
         $a[16] = $bioType{$a[3]};
     } elsif (defined($parent{$a[3]})) {
        my $parent = $parent{$a[3]};
        if (defined($bioType{$parent})) {
           $a[16] = $bioType{$parent};
        }
     }
  }
  #  "Gene type"
  if (length($a[19]) < 1) {
     if (defined($Type{$a[3]})) {
         $a[19] = $Type{$a[3]};
     } elsif (defined($parent{$a[3]})) {
        my $parent = $parent{$a[3]};
        if (defined($Type{$parent})) {
           $a[19] = $Type{$parent};
        }
     }
  }
  if (defined($descr{$a[3]})) {
     $a[$sizeA] = $descr{$a[3]};
  } elsif (defined($parent{$a[3]})) {
     my $parent = $parent{$a[3]};
     if (defined($descr{$parent})) {
        $a[$sizeA] = $descr{$parent};
     }
  } else {
     $a[$sizeA] = "";
  }
  if (scalar(@a) == 20) {
    $a[20] = "n/a";
  }
  if (scalar(@a) != 21) {
    printf STDERR "incorred # of entries %d != 21 for %s\n", scalar(@a), $a[3];
    exit 255;
  }
  printf "%s\n", join("\t", @a);
}
close ($fh);
printf STDERR "### updated %d items of total %d - %s\n", $updatedNames, $totalItems, $gpFile;

__END__

00      NW_027257890.1
01      99779
02      105382
03      XM_071090490.1
04      0
05      +
06      99779
07      105239
08      0
09      2
10      58,349,
11      0,5254,
12      LOC139361723
13      cmpl
14      cmpl
15      0,1,
16
17      XM_071090490.1
18      LOC139361723
19

bigGenePredFields:

     1     string chrom;       "Reference sequence chromosome or scaffold"
     2     uint   chromStart;  "Start position in chromosome"
     3     uint   chromEnd;    "End position in chromosome"
     4     string name;        "Name or ID of item, ideally both human readable and unique"
     5     uint score;         "Score (0-1000)"
     6     char[1] strand;     "+ or - for strand"
     7     uint thickStart;    "Start of where display should be thick (start codon)"
     8     uint thickEnd;      "End of where display should be thick (stop codon)"
     9     uint reserved;       "RGB value (use R,G,B string in input file)"
    10     int blockCount;     "Number of blocks"
    11     int[blockCount] blockSizes; "Comma separated list of block sizes"
    12     int[blockCount] chromStarts; "Start positions relative to chromStart"
    13     string name2;       "Alternative/human readable name"
    14     string cdsStartStat; "Status of CDS start annotation (none, unknown, incomplete, or complete)"
    15     string cdsEndStat;   "Status of CDS end annotation (none, unknown, incomplete, or complete)"
    16     int[blockCount] exonFrames; "Reading frame of the start of the CDS region of the exon, in the direction of transcription (0,1,2), or -1 if there is no CDS region."
    17     string type;        "Transcript type"
    18     string geneName;    "Primary identifier for gene"
    19     string geneName2;   "Alternative/human readable gene name"
    20     string geneType;    "Gene type"


grep LOC139361723 GCF_043159975.1_mMacNem.hap1.attrs.txt
gene-LOC139361723       ID      gene-LOC139361723
gene-LOC139361723       Dbxref  GeneID:139361723
gene-LOC139361723       Name    LOC139361723
gene-LOC139361723       description     beta-defensin 109
gene-LOC139361723       gbkey   Gene
gene-LOC139361723       gene    LOC139361723
gene-LOC139361723       gene_biotype    protein_coding
gene-LOC139361723       Seqid   NW_027257890.1
gene-LOC139361723       Source  Gnomon
gene-LOC139361723       Type    gene
gene-LOC139361723       Start   99779
gene-LOC139361723       End     105382
gene-LOC139361723       Strand  +
XM_071090490.1  Parent  gene-LOC139361723
