#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1) {
  printf STDERR "usage: gffToIx.pl file.gff > file.ix.txt\n";
  printf STDERR "e.g.: ./gffToIx.pl GCF_000001405.31_GRCh38.p5_genomic.gff.gz > GCF_000001405.31_GRCh38.p5_genomic.ix.txt\n";
  exit 255;
}

my $gffFile = shift;

if ($gffFile =~ m/.gz$/) {
  open (FH, "zcat $gffFile|") or die "can not read $gffFile";
} else {
  open (FH, "<$gffFile|") or die "can not read $gffFile";
}

my %ids;   # key is an ID, value is a string of any keywords found on
           # this ID or on any record that declares this ID as a parent
my %parent;  # key is a parent name, value is a string with any keywords
             # in any records of this parent

while (my $line = <FH>) {
  chomp $line;
  $line =~ s/%2C//g;
  my @a = split('\t', $line);
  if (defined($a[0]) && defined($a[8])) {
    my @b = split(';', $a[8]);
    my %tagValue;
    for (my $i=0; $i < scalar(@b); ++$i) {
       my ($tag, $value) = split('=', $b[$i]);
       my $lcTag = lc($tag);
       $tagValue{$lcTag} = $value;
    }
    my $id = "";
    my $parent = "";
    if (defined($tagValue{"id"})) {
       $id = $tagValue{"id"};
       if (defined($tagValue{"parent"})) {
          $parent = $tagValue{"parent"};
       }
    } elsif (defined($tagValue{"parent"})) {
       printf STDERR "# no id but parent='%s'\n", $tagValue{"parent"};
       $id = $tagValue{"parent"};
       $parent = $tagValue{"parent"};
    } else {
       printf STDERR "# no id or parent: '%s'\n", $line;
    }
    if (length($id) > 0) {
      my $tagValue = $id;
      $tagValue .= " " . $tagValue{"parent"} if (defined($tagValue{"parent"}));
      $tagValue .= " " . $tagValue{"name"} if (defined($tagValue{"name"}));
      $tagValue .= " " . $tagValue{"locus_tag"} if (defined($tagValue{"locus_tag"}));
#       $tagValue .= " " . $tagValue{"note"} if (defined($tagValue{"note"}));
#       $tagValue .= " " . $tagValue{"product"} if (defined($tagValue{"product"}));
      $tagValue .= " " . $tagValue{"protein_id"} if (defined($tagValue{"protein_id"}));
      $tagValue .= " " . $tagValue{"transcript_id"} if (defined($tagValue{"transcript_id"}));
      $tagValue .= " " . $tagValue{"gene"} if (defined($tagValue{"gene"}));
      if (defined($tagValue{"dbxref"})) {
        my @xrefs = split(",", $tagValue{"dbxref"});
        foreach my $xref (@xrefs) {
           $xref =~ s/.*://;
           $tagValue .= " " . $xref;
        }
      }
      $tagValue =~ s/  */ /g;
# printf STDERR "%s\t%s\n", $id, $tagValue;
      if (defined($ids{$id})) {
         $ids{$id} .= " " . $tagValue;
      } else {
         $ids{$id} = $tagValue;
      }
      if (length($parent) > 0) {
        if ($parent ne $id) {
          if (defined($ids{$parent})) {
             $ids{$parent} .= " " . $tagValue;
          } else {
             $ids{$parent} = $tagValue;
          }
        }
      }
    }
  }
}
close (FH);

foreach my $id (sort keys %ids) {
  my $words = $ids{$id};
  my %uniq;
  my @a = split('\s+', $words);
  for (my $i = 0; $i < scalar(@a); ++$i) {
    $uniq{$a[$i]} += 1 if ($a[$i] ne $id);
  }
  my $spaceValue = sprintf("%s\t", $id);
  foreach my $word (sort keys %uniq) {
    printf "%s%s", $spaceValue, $word;
    $spaceValue = " ";
  }
  printf "\n" if ($spaceValue eq " ");
}

__END__

name=
locus_tag
note
product
protein_id

AMCR01022363.1  Genbank gene    2583    4789    .       -       .       ID=gene24577;Name=OXYTRI_24882;gbkey=Gene;gene_biotype=protein_coding;locus_tag=OXYTRI_24882
AMCR01022363.1  Genbank mRNA    2583    4789    .       -       .       ID=rna24577;Parent=gene24577;Note=Contig1491.1.g75.t1;gbkey=mRNA;product=Ubiquitin-like-specific protease 1A
AMCR01022363.1  Genbank exon    3898    4789    .       -       .       ID=id89202;Parent=rna24577;Note=Contig1491.1.g75.t1;gbkey=mRNA;product=Ubiquitin-like-specific protease 1A
AMCR01022363.1  Genbank exon    2583    3862    .       -       .       ID=id89203;Parent=rna24577;Note=Contig1491.1.g75.t1;gbkey=mRNA;product=Ubiquitin-like-specific protease 1A
AMCR01022363.1  Genbank CDS     3898    4789    .       -       0       ID=cds24577;Parent=rna24577;Dbxref=NCBI_GP:EJY64204.1;Name=EJY64204.1;Note=Contig1491.1.g75.t1;gbkey=CDS;product=Ubiquitin-like-specific protease 1A;protein_id=EJY64204.1;transl_table=6
AMCR01022363.1  Genbank CDS     2583    3862    .       -       2       ID=cds24577;Parent=rna24577;Dbxref=NCBI_GP:EJY64204.1;Name=EJY64204.1;Note=Contig1491.1.g75.t1;gbkey=CDS;product=Ubiquitin-like-specific protease 1A;protein_id=EJY64204.1;transl_table=6

      printf "# id=%s", $tagValue{"id"};
      foreach my $tag (keys %tagValue) {
        next if ($tag =~ m/id/);
        printf " %s=%s", $tag, $tagValue{$tag};
      }
gff3ToGenePred -warnAndContinue -useName \
   -attrsOut=${chrName}.geneAttrs.ncbi.txt \
../../../genbank/GCA_000711775.1_Oxytricha_MIC_v2.0/GCA_000711775.1_Oxytricha_MIC_v2.0_genomic.gff.gz \
stdout | gzip -c > genbank.${chrName}.gp.gz

# id=cds789 parent=rna789 name=KEJ82394.1 transl_table=6 dbxref=NCBI_GP:KEJ82394.1 gbkey=CDS product=MT-A70 family protein
# id=cds790 parent=rna790 name=KEJ82393.1 transl_table=6 dbxref=NCBI_GP:KEJ82393.1 gbkey=CDS product=TLDc domain-containing protein
# id=cds795 parent=rna795 name=KEJ82388.1 transl_table=6 dbxref=NCBI_GP:KEJ82388.1 gbkey=CDS product=MT-A70 family protein
# id=cds808 parent=rna808 name=KEJ82375.1 transl_table=6 dbxref=NCBI_GP:KEJ82375.1 gbkey=CDS product=Histone H4
# id=cds809 parent=rna809 name=KEJ82374.1 transl_table=6 dbxref=NCBI_GP:KEJ82374.1 gbkey=CDS product=histone H3


Survey of tags:

  29142 id
  29142 gbkey
  26621 dbxref
  25720 strain
  25720 note
  25720 mol_type
  25720 dev-stage
   2612 product
   2612 parent
   1711 name
    901 transl_table
    901 protein_id
    810 locus_tag
    810 gene_biotype

#############################################################
Parent	2971885
protein_id	1233127
transcript_id	1719452

#############################################################
D_loop	1
Is_circular	1
country	1
isolation-source	1
long_terminal_repeat	1
note	1
tissue-type	1
lxr_locAcc_currStat_35	2
exon_number	3
transl_table	13
rRNA	23
C_gene_segment	43
codons	44
D_gene_segment	61
merge_aligner	124
J_gene_segment	128
map	365
chromosome	516
genome	517
mol_type	517
region	598
tRNA	624
V_gene_segment	682
anticodon	1287
transl_except	1392
start_range	1480
lxr_locAcc_currStat_120	1638
end_range	1911
primary_transcript	2027
Gap	2266
cDNA_match	10457
partial	10987
consensus_splices	11355
exon_identity	11355
identity	11355
idty	11355
matches	11355
product_coverage	11355
splices	11355
transcript	12388
pseudo	14949
blast_aligner	17632
blast_score	17632
hsp_percent_coverage	17632
bit_score	17756
common_component	17756
e_value	17756
filter_score	17756
matchable_bases	17756
matched_bases	17756
pct_identity_gapopen_only	17756
assembly_bases_aln	18461
assembly_bases_seq	18461
match	18766
gene_synonym	27693
score	27911
weighted_identity	27911
Target	29223
for_remapping	29223
gap_count	29223
num_ident	29223
num_mismatch	29223
pct_coverage	29223
pct_coverage_hiqual	29223
pct_identity_gap	29223
pct_identity_ungap	29223
rank	29223
ncRNA	30732
description	42045
gene	56387
gene_biotype	56387
mRNA	102619
exception	107447
ncrna_class	137315
Note	393090
CDS	1234728
Name	1434809
exon	1587830
product	2959969
gene	3028326
Dbxref	3028843
gbkey	3028872
ID	3058095
