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
$urlPath =~ s#genomes/genbank/##;;
$urlPath =~ s#/.*##;;
# genbank/vertebrate_mammalian/Tursiops_truncatus/latest_assembly_versions/GCA_000151865.3_Ttru_1.4/

my %taxIdCommonName;  # key is taxId, value is common name
                      # from NCBI taxonomy database dump
open (FH, "</hive/data/inside/ncbi/genomes/genbank/scripts/taxId.comName.tab") or die "can not read taxId.comName.tab";
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
my $asmType = "(n/a)";
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
     next if ($asmType !~ m#\(n/a#);
     $line =~ s/.*assembly\s+type:\s+//i;
     $asmType = $line;
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
  if ($line =~ m/genbank\s+assembly\s+accession:\s+/i) {
     next if ($asmAccession !~ m#\(n/a#);
     $line =~ s/.*genbank\s+assembly\s+accession:\s+//i;
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
printf STDERR "%s\t", $asmType;
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
<b>GenBank accession ID:</b> <a href=\"http://www.ncbi.nlm.nih.gov/assembly/%s\" target=\"_blank\">%s</a><br>
<b>Assembly FTP location:</b> <a href=\"ftp://ftp.ncbi.nlm.nih.gov/%s\" target=\"_blank\">%s</a><br>
<b>UCSC downloads directory:</b> <a href=\"http://genome-test.cse.ucsc.edu/~hiram/hubs/genbank/%s/%s/\" target=\"_blank\">%s_%s</a><br>
</p>\n", $commonName, $orgName, $taxId, $taxId, $submitter, $asmDate, $asmType,
  $asmLevel, $bioSample, $bioSample, $asmAccession, $asmAccession, $ftpName, $asmName, $urlPath, $urlDirectory, $asmAccession, $asmName;

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

/hive/data/outside/ncbi/genomes/genbank/vertebrate_mammalian/Sus_scrofa/all_assembly_versions/GCA_000003025.4_Sscrofa10.2/GCA_000003025.4_Sscrofa10.2_assembly_report.txt

# Assembly Name:  Sscrofa10.2
# Organism name:  Sus scrofa
# Taxid:          9823
# Submitter:      The Swine Genome Sequencing Consortium (SGSC)
# Date:           2011-9-7
# BioSample:      SAMN02953785
# Assembly type:  haploid
# Release type:   major
# Assembly level: Chromosome
# Genome representation: full
# GenBank Assembly Accession: GCA_000003025.4 (latest)
# RefSeq Assembly Accession: GCF_000003025.5 (species-representative latest)
# RefSeq Assembly and GenBank Assemblies Identical: no
#
## Assembly-Units:
## GenBank Unit Accession       RefSeq Unit Accession   Assembly-Unit name
## GCA_000000215.4      GCF_000000215.5 Primary Assembly
##      GCF_000090915.1 non-nuclear
#
# Ordered by chromosome/plasmid; the chromosomes/plasmids are followed by
# unlocalized scaffolds.
# Unplaced scaffolds are listed at the end.
# RefSeq is equal or derived from GenBank object.
#
# Sequence-Name Sequence-Role   Assigned-Molecule       Assigned-Molecule-Location/Type GenBank-Accn    Relationship    RefSeq-Accn     Assembly-Unit
chr1    assembled-molecule      1       Chromosome      CM000812.4      =       NC_010443.4     Primary Assembly
chr2    assembled-molecule      2       Chromosome      CM000813.4      =       NC_010444.3     Primary Assembly
chr3    assembled-molecule      3       Chromosome      CM000814.4      =       NC_010445.3     Primary Assembly

