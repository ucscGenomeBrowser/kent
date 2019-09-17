#!/usr/bin/env perl

use strict;
use warnings;

##############################################################################
# adjust values here for new patch level
my $patchLevel = 13;
my $ftpDirectory = "GRCh37.p13";
my $asmId = "GCA_000001405.14_GRCh37.p${patchLevel}";
my $patchOrdinal = "thirteenth";
##############################################################################


my $haploCount = `cat hg19Patch${patchLevel}Haplotypes.bed | wc -l`;
chomp $haploCount;
my $patchCount = `cat hg19Patch${patchLevel}Patches.bed | wc -l`;
chomp $patchCount;

sub beginHtml() {

printf "<h2>Description</h2>\n";

printf "<p>
   Patch release $patchLevel is the $patchOrdinal patch release for the GRCh37 assembly.
   This is a minor release of GRCh37 that does not disrupt the coordinate
   system in the reference sequence GRCh37.
   All but four patch scaffolds from patch releases 1-12 were retained.<br>
   Between patch release 12 and patch release 13:<br>
   10 patch scaffolds of type FIX were added.<br>
   14 patch scaffolds of type FIX were updated.<br>
    1 patch scaffold of type NOVEL was updated.<br>
</p>\n\n", $patchLevel - 1;

printf "<p>
   Total patch scaffolds in this patch release: %d<br>
   Patch scaffolds of type FIX: %d<br>
   Patch scaffolds of type NOVEL: %d<br>
</p>\n\n", $patchCount+$haploCount, $patchCount, $haploCount;


printf "<h2>Definitions</h2>\n";
printf "<ul>\n
<li>Genome Patch: A sequence contig/scaffold that corrects sequence
    in a major release of the genome, or adds sequence to it.</li>
<li>FIX patch: A patch that corrects sequence or reduces an assembly gap
    in a given major release. FIX patch sequences are meant to be
    incorporated into the primary or existing alt-loci assembly units at
    the next major release, and their accessions will then be
    deprecated.</li>
<li>NOVEL patch: A patch that adds sequence to a major release. Typically,
    NOVEL patch sequences are meant to be incorporated into the assembly
    as new alternate loci at the next major release, and their
    accessions will not be deprecated  (UCSC term: \"new haplotype sequence\").</li>
</ul>\n\n";

printf "<h2>Patch release $patchLevel summary table</h2>\n";
}

sub endHtml() {
printf "<h2>References</h2>\n";
printf "<p>
Data obtained from the
   <a href=\"https://www.ncbi.nlm.nih.gov/projects/genome/assembly/grc/info/patches.shtml\" target=\"_blank\">
   Genome Reference Consortium:</a>&nbsp;&nbsp;
   <a href=\"ftp://ftp.ncbi.nlm.nih.gov/genomes/archive/old_genbank/Eukaryotes/vertebrates_mammals/Homo_sapiens/${ftpDirectory}/\" target=\"_blank\">FTP</a>.
</p>\n";
}

my %seqName;
my %seqRole;
# genbank/GCA_000001405.14_GRCh37.p13_assembly_report.txt
# Sequence-Name	Sequence-Role	Assigned-Molecule	Assigned-Molecule-Location/Type	GenBank-Accn	Relationship	RefSeq-Accn	Assembly-Unit
# 1	assembled-molecule	1	Chromosome	CM000663.1	=	NC_000001.10	Primary Assembly
open (FH, "<genbank/${asmId}_assembly_report.txt") or die "can not read genbank/${asmId}_assembly_report.txt";
while (my $line = <FH>) {
   next if ($line =~ m/^#/);
   chomp $line;
   my ($seqName, $seqRole, $molecule, $locationType, $genbankAccn, $relationship, $refSeqAccn, $asmUnit) = split('\t', $line);
   $seqName{$genbankAccn} = $seqName;
   $seqRole{$genbankAccn} = $seqRole;
}
close (FH);

my %region;

open (FH, "<genbank/${asmId}_assembly_regions.txt") or die "can not read genbank/${asmId}_assembly_regions.txt";
while (my $line = <FH>) {
   next if ($line =~ m/^#/);
  chomp $line;
  my ($regionName, $chrom, $chrStart, $chrEnd, $scaffoldRole, $scaffoldGenbank, $scaffoldRefSeq, $assemblyUnit) = split('\t', $line);
  $region{$scaffoldGenbank} = $regionName;
}
close (FH);

my $partCount = 0;
beginHtml;

printf "<table border=1>\n";
printf "<tr><th>GRCh37 reference<br>location</th><th>NCBI Entrez<br>nucleotide record</th><th>GRC region<br>name</th><th>sequence<br>name</th><th>patch<br>type</th></tr>\n";
open (FH, "sort -k1,1 -k2,2n ncbi.patchLocations.bed|") or die "can not read ncbi.patchLocations.bed";
while (my $line = <FH>) {
  chomp $line;
  my ($chr, $start, $end, $name) = split('\t', $line);
  my $ncbi = $name;
  $ncbi =~ s/_fix|_alt//;
  $ncbi =~ s/chr.*_//;
  $ncbi =~ s/v/./;
  printf "<tr><td><a href=\"../cgi-bin/hgTracks?db=hg19&position=%s:%d-%d\">%s:%d-%d</a></td>\n", $chr, $start+1, $end, $chr, $start+1, $end;
  printf "    <td><a href=\"https://www.ncbi.nlm.nih.gov/nuccore/%s?report=genbank\" target=\"_blank\">%s</a></td>\n", $ncbi, $name;
  printf "    <td><a href=\"https://www.ncbi.nlm.nih.gov/projects/genome/assembly/grc/region.cgi?name=%s&asm=GRCh37.p${patchLevel}\" target=\"_blank\">%s</a></td>\n", $region{$ncbi}, $region{$ncbi};
  printf "    <td>%s</td><td>%s</td>\n</tr>\n", $seqName{$ncbi}, $seqRole{$ncbi};
  ++$partCount;
}
close (FH);
printf "</table>\n";

endHtml;

printf STDERR "total items: %d\n", $partCount;
