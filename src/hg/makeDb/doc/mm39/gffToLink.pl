#!/usr/bin/env perl

use strict;
use warnings;

open (my $fh, "-|", "grep 'ID=gene-' genome.gff.NC_005089.1.tsv") or die "can not grep genome.gff.NC_005089.1.tsv";
while (my $line = <$fh>) {
  chomp $line;
  my @a = split('\t', $line);
  my @b = split(';', $a[8]);
#  printf "%s\t%s\n", $a[0], $b[0];
  my $id = "";
  my $name = "";
  my $product = "";
  my $mrnaAcc = "";
  my $protAcc = "";
  my $genbank = "";
  my $hgnc = "";
  my $gbkey = "";
  my $note = "";
  foreach my $tagVal (@b) {
    my @c = split('=', $tagVal, 2);
    if ( $c[0] eq "ID" ) {
	$id = $c[1];
	$id =~ s/cds-//;
	$id =~ s/gene-//;
	$mrnaAcc = $id;
    } elsif ($c[0] eq "gene") {
	$name = $c[1];
    } elsif ($c[0] eq "product") {
	$product = $c[1];
    } elsif ($c[0] eq "protein_id") {
	$protAcc = $c[1];
    } elsif ($c[0] eq "gbkey") {
	$gbkey = $c[1];
    } elsif ($c[0] eq "Note") {
	$note = $c[1];
    } elsif ($c[0] eq "Dbxref") {
        my @d = split(',', $c[1]);
	foreach my $xref (@d) {
	    my @e = split(':', $xref, 2);
	    if ( $e[0] eq "GenBank") {
		$genbank = $e[1];
	    } elsif ( $e[0] eq "HGNC" ) {
		$hgnc = $e[1];
		$hgnc =~ s/HGNC://;
	    }
	}
    }
  }
  printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", $id, "Unknown", $name, $product, $mrnaAcc, $protAcc, "", "", $hgnc, $genbank, "", $gbkey, "", "", "", "", $note, "", "";
}
close ($fh);

__END__
  my $id = "";
  my $name = "";
  my $product = "";
  my $mrnaAcc = "";
  my $protAcc = "";
  my $genbank = "";
  my $hgnc = "";
  my $gbkey = "";
  my $note = "";

NC_012920.1     ID=cds-YP_003024026.1
ID      cds-YP_003024026.1
Parent  rna-ND1
Dbxref  GenBank:YP_003024026.1,GeneID:4535,HGNC:HGNC:7455,MIM:516000
Name    YP_003024026.1
Note    TAA stop codon is completed by the addition of 3' A residues to the mRNA
gbkey   CDS
gene    ND1
product NADH dehydrogenase subunit 1
protein_id      YP_003024026.1
transl_except   (pos:4261..4262%2Caa:TERM)
transl_table    2


chrMT.catchUp.refLink.tab
fromNcbiRefLink.txt
fromNcbiRefSeq.txt
genome.gff.YP_.tsv
ncbiRefSeqLink.desc.txt
someData.sh
twoTable.data.txt

NC_012920.1	RefSeq	CDS	3307	4262	.	+	0	ID=cds-YP_003024026.1;Parent=rna-ND1;Dbxref=GenBank:YP_003024026.1,GeneID:4535,HGNC:HGNC:7455,MIM:516000;Name=YP_003024026.1;Note=TAA stop codon is completed by the addition of 3' A residues to the mRNA;gbkey=CDS;gene=ND1;product=NADH dehydrogenase subunit 1;protein_id=YP_003024026.1;transl_except=(pos:4261..4262%2Caa:TERM);transl_table=2

+--------------+--------------+------+-----+---------+-------+
| Field        | Type         | Null | Key | Default | Extra |
+--------------+--------------+------+-----+---------+-------+
| id           | varchar(255) | NO   | PRI | NULL    |       |
| status       | varchar(255) | NO   |     | NULL    |       |
| name         | varchar(255) | NO   | MUL | NULL    |       |
| product      | varchar(255) | NO   |     | NULL    |       |
| mrnaAcc      | varchar(255) | NO   | MUL | NULL    |       |
| protAcc      | varchar(255) | NO   | MUL | NULL    |       |
| locusLinkId  | varchar(255) | NO   |     | NULL    |       |
| omimId       | varchar(255) | NO   |     | NULL    |       |
| hgnc         | varchar(255) | NO   |     | NULL    |       |
| genbank      | varchar(255) | NO   |     | NULL    |       |
| pseudo       | varchar(255) | NO   |     | NULL    |       |
| gbkey        | varchar(255) | NO   |     | NULL    |       |
| source       | varchar(255) | NO   |     | NULL    |       |
| gene_biotype | varchar(255) | NO   |     | NULL    |       |
| gene_synonym | varchar(255) | NO   |     | NULL    |       |
| ncrna_class  | varchar(255) | NO   |     | NULL    |       |
| note         | longblob     | NO   |     | NULL    |       |
| description  | longblob     | NO   |     | NULL    |       |
| externalId   | varchar(255) | NO   |     | NULL    |       |
+--------------+--------------+------+-----+---------+-------+

