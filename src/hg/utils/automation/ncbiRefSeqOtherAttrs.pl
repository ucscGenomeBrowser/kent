#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/ncbiRefSeqOtherAttrs.pl instead.

# This script reads predigested attributes (gff3ToGenePred -attrsOut=...) and BED 12 input.
# It outputs BED 12 + with attributes in extra fields.

use warnings;
use strict;

my $base = $0;
$base =~ s/^(.*\/)?//;

my $asFile = "ncbiRefSeqOther.as";

sub usage {
  print STDERR "
usage: $base \$db.other.bed \$asmId.attrs.txt > \$db.other.extras.bed

Adds GFF3 attributes to BED to make BED+ for ncbiRefSeqOther bigBed.
Generates $asFile autoSql file.
See doNcbiRefSeq.pl.
";
} # usage

# NCBI GFF3 attribute (or derivative) names, bigBed column names and descriptions
# (used as hgc table labels), in the order of extra fields:
my @attributes = (['gene',           'gene',           "Gene name"],
                  ['GeneID',         'GeneID',         "Entrez Gene"],
                  ['MIM',            'MIM',            "OMIM"],
                  ['HGNC',           'HGNC',           "HGNC"],
                  ['MGI',            'MGI',            "MGI"],
                  ['WormBase',       'WormBase',       "WormBase"],
                  ['XenBase',        'XenBase',        "XenBase"],
                  ['BGD',            'BGD',            "BGD"],
                  ['RGD',            'RGD',            "RGD"],
                  ['SGD',            'SGD',            "SGD"],
                  ['ZFIN',           'ZFIN',           "ZFIN"],
                  ['FlyBase',        'FlyBase',        "FlyBase"],
                  ['miRBase',        'miRBase',        "miRBase"],
                  ['description',    'description',    "Description"],
                  ['Note',           'Note',           "Note"],
                  ['exception',      'exception',      "Exceptional properties"],
                  ['product',        'product',        "Gene product"],
                  ['gene_synonym',   'geneSynonym',    "Gene synonyms"],
                  ['model_evidence', 'modelEvidence',  "Supporting evidence for gene model"],
                  ['gbkey',          'gbkey',          "Feature type"],
                  ['gene_biotype',   'geneBiotype',    "Gene biotype"],
                  ['pseudo',         'pseudo',         "'true' if pseudogene"],
                  ['partial',        'partial',        "'true' if partial"],
                  ['anticodon',      'anticodon',      "Anticodon position within tRNA"],
                  ['codons',         'codons',         "Number of codons in anticodon range"],
                  ['start_range',    'startRange',     "Start range on genome"],
                  ['end_range',      'endRange',       "End range on genome"],
                  ['ID',             'ID',             "Unique ID in RefSeq GFF3"] );


my @attrOrder = map { $_->[0] } @attributes;

# Command line args
my $bedFile = shift @ARGV;
my $attrsFile = shift @ARGV;
if (not (defined $bedFile && defined $attrsFile && scalar @ARGV == 0)) {
  usage();
  exit 1;
}

# Map attribute names to indexes in extra field order:
my %attrToIx = ();
my @attrFound = ();
for (my $i = 0;  $i <= $#attrOrder;  $i++) {
  $attrToIx{$attrOrder[$i]} = $i;
  $attrFound[$i] = 0;
}

# Map item IDs to extra columns:
my %itemAttrs = ();
my %ignoredAttrs = ();
my %geneToId = ();
my %idToParent = ();
open(my $ATTRS, $attrsFile) || die "Can't open attributes file '$attrsFile': $!\n";
while (<$ATTRS>) {
  chomp;
  my ($id, $attr, $val) = split("\t");
  if ($attr eq 'Dbxref') {
    # Dbxref is one attribute, but split it up into multiple output columns for URL generation
    my @xrefs = split(',', $val);
    foreach my $xref (@xrefs) {
      foreach my $source qw(GeneID MIM HGNC MGI miRBase WormBase XenBase BGD RGD SGD ZFIN FLYBASE) {
        if ($xref =~ s/^$source://) {
          my $ix = $attrToIx{$source};
          $itemAttrs{$id}->[$ix] = $xref if (! defined $itemAttrs{$id}->[$ix]);
          $attrFound[$ix] = 1;
        }
      }
    }
  } elsif ($attr eq 'Parent') {
    $idToParent{$id} = $val;
  } else {
    my $ix = $attrToIx{$attr};
    if (defined $ix) {
      if ($attr eq 'gene') {
        $geneToId{$val} = $id;
      }
      $itemAttrs{$id}->[$ix] = $val;
      $attrFound[$ix] = 1;
    } else {
      $ignoredAttrs{$attr}++;
    }
  }
}
close($ATTRS);

foreach my $attr (sort { $ignoredAttrs{$b} <=> $ignoredAttrs{$a} } keys %ignoredAttrs) {
  warn "Ignored attribute '$attr' " . $ignoredAttrs{$attr} . " times\n";
}

# Make autoSql
open (my $AS, ">$asFile") || die "Can't open $asFile for writing: $!\n";
print $AS <<_EOF_
table ncbiRefSeqOther
"Additional information for NCBI 'Other' annotation"
    (
    string chrom;      "Chromosome (or contig, scaffold, etc.)"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Placeholder for BED format (score 0-1000)"
    char[1] strand;    "+ or -"
    uint thickStart;   "CDS start/end in chromosome if coding"
    uint thickEnd;     "CDS start/end in chromosome if coding"
    uint reserved;     "Placeholder for BED format (itemRgb)"
    int blockCount;    "Number of alignment blocks"
    int[blockCount] blockSizes; "Comma separated list of block sizes"
    int[blockCount] chromStarts; "Block start positions relative to chromStart"

_EOF_
;
for (my $ix = 0;  $ix <= $#attributes;  $ix++) {
  if ($attrFound[$ix]) {
    my ($attr, $colName, $desc) = @{$attributes[$ix]};
    if ($colName eq "Note" || $colName eq "geneSynonym") {
      print $AS "    lstring $colName;   \"$desc\"\n";
    } else {
      print $AS "    string $colName;   \"$desc\"\n";
    }
  }
}
print $AS ")\n";
close($AS);

# Make a list of indexes of empty columns, in descending order, for splicing out:
for (my $ix = 0;  $ix <= $#attrOrder;  $ix++) {
  if (! $attrFound[$ix]) {
    warn "No values found for $attrOrder[$ix]; removing column from output.\n";
  }
}

# Read BED and append extra columns:
open(my $BED, $bedFile) || die "Can't open BED file '$bedFile': $!\n";
while (<$BED>) {
  chomp;
  my @bedCols = split;
  my $id = $bedCols[3];
  my $extraCols = $itemAttrs{$id};
  if (! defined $extraCols) {
    $id = geneToId{$id};
    $extraCols = $itemAttrs{$id};
  }
  die "No attributes for bed name '$bedCols[3]' (id '$id')" unless defined $extraCols;
  foreach my $ix (0..$#attrOrder) {
    if ($attrFound[$ix]) {
      # If the desired attribute isn't there for $id, but $id has ancestors, look for attr there
      my $parentId = $idToParent{$id};
      while (! defined $extraCols->[$ix] && defined $parentId) {
        my $parent = $itemAttrs{$parentId} || $itemAttrs{$geneToId{$parentId}};
        if (defined $parent) {
          if (defined $parent->[$ix]) {
            $extraCols->[$ix] = $parent->[$ix];
          } else {
            $parentId = $idToParent{$parentId};
          }
        } else {
          warn "Can't find parent for id '$id'\n";
          last;
        }
      }
      if (! defined $extraCols->[$ix]) {
        $extraCols->[$ix] = "";
      }
    }
  }
  print join ("\t", @bedCols);
  # Print out only non-empty extra columns.
  foreach my $ix (0..$#attrOrder) {
    if ($attrFound[$ix]) {
      print "\t" . $extraCols->[$ix];
    }
  }
  print "\n";
}
close($BED);
