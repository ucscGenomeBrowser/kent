#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/ncbiRefSeqOtherAttrs.pl instead.

# This script reads predigested attributes (gff3ToGenePred -attrsOut=...) and BED 12 input.
# It outputs BED 12 + with attributes in extra fields.

#*** Make command line args:
my $bedFile = "hg38.other.bed";
my $attrsFile = "GCF_000001405.37_GRCh38.p11.attrs.txt";

my $base = $0;
$base =~ s/^(.*\/)?//;

sub usage {
  print STDERR "
usage: $base \$db.other.bed \$asmId.attrs.txt > \$db.other.extras.bed

Adds GFF3 attributes to BED to make BED+ for ncbiRefSeqOther bigBed.
See doNcbiRefSeq.pl.
";
} # usage

# NCBI GFF3 attribute (or derivative) names, in the order of extra fields:
my @attrOrder = qw( gene
                    GeneID
                    MIM
                    HGNC
                    miRBase
                    description
                    Note
                    exception
                    product
                    gene_synonym
                    model_evidence
                    gbkey
                    gene_biotype
                    pseudo
                    partial
                    anticodon
                    codons
                    start_range
                    end_range
                    ID
                 );

# Command line args
my $bedFile = shift @ARGV;
my $attrsFile = shift @ARGV;
if (not (defined $bedFile && defined $attrsFile && scalar @ARGV == 0)) {
  usage();
  exit 1;
}

# Map attribute names to indexes in extra field order:
my %attrToIx = ();
for (my $i = 0;  $i <= $#attrOrder;  $i++) {
  $attrToIx{$attrOrder[$i]} = $i;
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
      foreach my $source qw(GeneID MIM HGNC miRBase) {
        if ($xref =~ s/^$source://) {
          my $ix = $attrToIx{$source};
          $itemAttrs{$id}->[$ix] = $xref if (! defined $itemAttrs{$id}->[$ix]);
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
    } else {
      $ignoredAttrs{$attr}++;
    }
  }
}
close($ATTRS);

foreach my $attr (sort { $ignoredAttrs{$b} <=> $ignoredAttrs{$a} } keys %ignoredAttrs) {
  warn "Ignored attribute '$attr' " . $ignoredAttrs{$attr} . " times\n";
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
  print join ("\t", @bedCols, @{$extraCols}) . "\n";
}
close($BED);

