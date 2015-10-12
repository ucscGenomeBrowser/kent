#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;

my @months = qw( 0 Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec );

sub usage() {
  printf STDERR "usage: gatewayPage.pl <pathTo>/*assembly_report.txt\n";
  printf STDERR "output is to stdout, redirect to file: > description.html\n";
  exit 255;
}

# typical reference:
#   ${inside}/scripts/gatewayPage.pl ${outside}/${asmReport} \
#       > "${inside}/${D}/${B}.description.html" \
#       2> "${inside}/${D}/${B}.names.tab"


my $argc = scalar(@ARGV);

if ($argc < 1) {
  usage;
}

my $asmReport = shift;
if ( ! -s $asmReport ) {
  printf STDERR "ERROR: can not find '$asmReport'\n";
  usage;
}

my $ftpName = dirname($asmReport);
$ftpName =~ s#/hive/data/outside/ncbi/##;
$ftpName =~ s#/hive/data/inside/ncbi/##;
my $urlDirectory = `basename $ftpName`;
chomp $urlDirectory;
my $urlPath = $ftpName;
my $asmType = "genbank";
$asmType = "refseq" if ( $urlPath =~ m#/refseq/#);
$urlPath =~ s#genomes/$asmType/##;;
$urlPath =~ s#/.*##;;

my %taxIdCommonName;  # key is taxId, value is common name
                      # from NCBI taxonomy database dump
open (FH, "<$ENV{'HOME'}/kent/src/hg/utils/automation/genbank/taxId.comName.tab") or die "can not read taxId.comName.tab";
while (my $line = <FH>) {
  chomp $line;
  my ($taxId, $comName) = split('\t', $line);
  $taxIdCommonName{$taxId} = $comName;
}
close (FH);


my $submitter = "(n/a)";
my $asmName = "(n/a)";
my $orgName = "(n/a)";
my $taxId = "(n/a)";
my $asmDate = "(n/a)";
my $asmAccession = "(n/a)";
my $commonName = "(n/a)";
my $bioSample = "(n/a)";
my $descrAsmType = "(n/a)";
my $asmLevel = "(n/a)";

open (FH, "<$asmReport") or die "can not read $asmReport";
while (my $line = <FH>) {
  chomp $line;
  $line =~ s///g;
  if ($line =~ m/date:\s+/i) {
     next if ($asmDate !~ m#\(n/a#);
     $line =~ s/.*date:\s+//i;
     my ($year, $month, $day) = split('-',$line);
     $asmDate = sprintf("%02d %s %04d", $day, $months[$month], $year);
  }
  if ($line =~ m/biosample:\s+/i) {
     next if ($bioSample !~ m#\(n/a#);
     $line =~ s/.*biosample:\s+//i;
     $bioSample = $line;
  }
  if ($line =~ m/assembly\s+type:\s+/i) {
     next if ($descrAsmType !~ m#\(n/a#);
     $line =~ s/.*assembly\s+type:\s+//i;
     $descrAsmType = $line;
  }
  if ($line =~ m/assembly\s+level:\s+/i) {
     next if ($asmLevel !~ m#\(n/a#);
     $line =~ s/.*assembly\s+level:\s+//i;
     $asmLevel = $line;
  }
  if ($line =~ m/assembly\s+name:\s+/i) {
     next if ($asmName !~ m#\(n/a#);
     $line =~ s/.*assembly\s+name:\s+//i;
     $asmName = $line;
  }
  if ($line =~ m/organism\s+name:\s+/i) {
     next if ($orgName !~ m#\(n/a#);
     $line =~ s/.*organism\s+name:\s+//i;
     $orgName = $line;
  }
  if ($line =~ m/submitter:\s+/i) {
     next if ($submitter !~ m#\(n/a#);
     $line =~ s/.*submitter:\s+//i;
     $submitter = $line;
  }
  if ($line =~ m/$asmType\s+assembly\s+accession:\s+/i) {
     next if ($asmAccession !~ m#\(n/a#);
     $line =~ s/.*$asmType\s+assembly\s+accession:\s+//i;
     $asmAccession = $line;
     $asmAccession =~ s/ .*//;
  }
  if ($line =~ m/taxid:\s+/i) {
     next if ($taxId !~ m#\(n/a#);
     $line =~ s/.*taxid:\s+//i;
     $taxId = $line;
     if (exists($taxIdCommonName{$taxId})) {
       $commonName = $taxIdCommonName{$taxId};
     }
  }
}
close (FH);

$commonName = $orgName if ($commonName =~ m#\(n/a#);

printf STDERR "#taxId\tcommonName\tsubmitter\tasmName\torgName\tbioSample\tasmType\tasmLevel\tasmDate\tasmAccession\n";
printf STDERR "%s\t", $taxId;
printf STDERR "%s\t", $commonName;
printf STDERR "%s\t", $submitter;
printf STDERR "%s\t", $asmName;
printf STDERR "%s\t", $orgName;
printf STDERR "%s\t", $bioSample;
printf STDERR "%s\t", $descrAsmType;
printf STDERR "%s\t", $asmLevel;
printf STDERR "%s\t", $asmDate;
printf STDERR "%s\n", $asmAccession;

printf "<p>
<b>Common name: %s<br>
<b>Taxonomic name: %s, taxonomy ID: <a href=\"http://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?id=%s\" target=_\"_blank\"> %s</a><br>
<b>Sequencing/Assembly provider ID:</b> %s<br>
<b>Assembly date:</b> %s<br>
<b>Assembly type:</b> %s<br>
<b>Assembly level:</b> %s<br>
<b>Biosample:</b> <a href=\"http://www.ncbi.nlm.nih.gov/biosample/?term=%s\" target=\"_blank\"> %s</a><br>
<b>Assembly accession ID:</b> <a href=\"http://www.ncbi.nlm.nih.gov/assembly/%s\" target=\"_blank\">%s</a><br>
<b>Assembly FTP location:</b> <a href=\"ftp://ftp.ncbi.nlm.nih.gov/genomes/all/%s\" target=\"_blank\">%s</a><br>
<b>UCSC downloads directory:</b> <a href=\"http://genome-test.cse.ucsc.edu/~hiram/hubs/$asmType/%s/%s/\" target=\"_blank\">%s_%s</a><br>
</p>\n", $commonName, $orgName, $taxId, $taxId, $submitter, $asmDate, $descrAsmType,
  $asmLevel, $bioSample, $bioSample, $asmAccession, $asmAccession, $urlDirectory, $asmName, $urlPath, $urlDirectory, $asmAccession, $asmName;

printf "<hr>
<p>
<b>Search the assembly:</b>
<ul>
<li>
<b>By position or search term: </b> Use the &quot;position or search term&quot;
box to find areas of the genome associated with many different attributes, such
as a specific chromosomal coordinate range; mRNA, EST, or STS marker names; or
keywords from the GenBank description of an mRNA.
<a href=\"http://genome.ucsc.edu/goldenPath/help/query.html\">More information</a>, including sample queries.</li>
<li>
<b>By gene name: </b> Type a gene name into the &quot;search term&quot; box,
choose your gene from the drop-down list, then press &quot;submit&quot; to go 
directly to the assembly location associated with that gene.
<a href=\"http://genome.ucsc.edu/goldenPath/help/geneSearchBox.html\">More information</a>.</li>
<li>
<b>By track type: </b> Click the &quot;track search&quot; button
to find Genome Browser tracks that match specific selection criteria.
<a href=\"http://genome.ucsc.edu/goldenPath/help/trackSearch.html\">More information</a>.</li>
</ul>
</p>
<hr>\n";

__END__

/hive/data/outside/ncbi/genomes/refseq/vertebrate_mammalian/Homo_sapiens/all_assembly_versions/GCF_000001405.30_GRCh38.p4/GCF_000001405.30_GRCh38.p4_assembly_report.txt

# Assembly Name:  GRCh38.p4
# Description:    Genome Reference Consortium Human Build 38 patch release 4 (GRCh38.p4)
# Organism name:  Homo sapiens (human)
# Taxid:          9606
# Submitter:      Genome Reference Consortium
# Date:           2015-6-25
# Assembly type:  haploid-with-alt-loci
# Release type:   patch
# Assembly level: Chromosome
# Genome representation: full
# GenBank Assembly Accession: GCA_000001405.19 (latest)
# RefSeq Assembly Accession: GCF_000001405.30 (latest)
# RefSeq Assembly and GenBank Assemblies Identical: yes
#
## Assembly-Units:
## GenBank Unit Accession       RefSeq Unit Accession   Assembly-Unit name
## GCA_000001305.2      GCF_000001305.14        Primary Assembly
## GCA_000005045.17     GCF_000005045.16        PATCHES
## GCA_000001315.2      GCF_000001315.2 ALT_REF_LOCI_1

