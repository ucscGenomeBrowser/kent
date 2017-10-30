#!/usr/bin/env perl

use strict;
use warnings;

my $patchLevel = 4;
my $patchOrdinal = "fourth";
my $asmName = "GCA_000001635.6_GRCm38.p${patchLevel}";

my $asmReport="genbank/${asmName}_assembly_report.txt";
my $regions="genbank/${asmName}_assembly_regions.txt";


my $haploCount = `cat mm10Patch${patchLevel}Haplotypes.bed | wc -l`;
chomp $haploCount;
my $patchCount = `cat mm10Patch${patchLevel}Patches.bed | wc -l`;
chomp $patchCount;

#   6 mm10Patch3Haplotypes.bed
#   33 mm10Patch3Patches.bed

sub beginHtml() {

printf "<h2>Description</h2>\n";

printf "<p>
   Patch release $patchLevel is the $patchOrdinal patch release for the GRCm38 assembly.
   Patch release $patchLevel includes all contents that were in patch release %d.
   This is a minor release of GRCm38 that does not disrupt the coordinate
   system in the reference sequence GRCm38.
</p>\n\n", $patchLevel - 1;

printf "<p>
   Total patch scaffolds in this patch release: %d<br>
   Patch scaffolds of type FIX: %d<br>
   Patch scaffolds of type NOVEL: %d<br>
</p>\n\n", $patchCount+$haploCount, $patchCount, $haploCount;


printf "<h2>Definitions</h2>\n";
printf "<ul>\n
<li>ALT scaffold: A sequence contig/scaffold that is a haplotype sequence
    to the reference genome indicated location.</li>
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
   <a href=\"ftp://ftp.ncbi.nlm.nih.gov/genomes/genbank/vertebrate_mammalian/Mus_musculus/all_assembly_versions/${asmName}/\" target=\"_blank\">FTP</a>.
</p>\n";
}

my %seqName;
my %seqRole;
open (FH, "<$asmReport") or die "can not read $asmReport";
while (my $line = <FH>) {
   next if ($line =~ m/^#/);
   chomp $line;
   my ($seqName, $seqRole, $molecule, $locationType, $genbankAccn, $relationship, $refSeqAccn, $asmUnit) = split('\t', $line);
   $seqName{$genbankAccn} = $seqName;
   $seqRole{$genbankAccn} = $seqRole;
}
close (FH);

my %region;

open (FH, "<$regions") or die "can not read $regions";
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
printf "<tr><th>GRCm38 reference<br>location</th><th>NCBI Entrez<br>nucleotide record</th><th>GRC region<br>name</th><th>sequence<br>name</th><th>patch<br>type</th></tr>\n";
open (FH, "sort -k1,1 -k2,2n patchLocations.bed|") or die "can not read patchLocations.bed";
while (my $line = <FH>) {
  chomp $line;
  my ($chr, $start, $end, $name) = split('\t', $line);
  my $ncbi = $name;
  $ncbi =~ s/_fix|_alt//;
  $ncbi =~ s/chr.*_//;
  if ($ncbi =~ m/v[0-9]+$/) {
     $ncbi =~ s/v/./;
  } else {
     $ncbi .= ".1";
  }
# printf STDERR  "# checking for ncbi: '%s'\n", $ncbi;
  printf "<tr><td><a href=\"../cgi-bin/hgTracks?db=mm10&position=%s:%d-%d\">%s:%d-%d</a></td>\n", $chr, $start+1, $end, $chr, $start+1, $end;
  printf "    <td><a href=\"https://www.ncbi.nlm.nih.gov/nuccore/%s\" target=\"_blank\">%s</a></td>\n", $ncbi, $name;
  printf "    <td><a href=\"https://www.ncbi.nlm.nih.gov/projects/genome/assembly/grc/region.cgi?name=%s&asm=GRCm38.p${patchLevel}\" target=\"_blank\">%s</a></td>\n", $region{$ncbi}, $region{$ncbi};
  printf "    <td>%s</td><td>%s</td>\n</tr>\n", $seqName{$ncbi}, $seqRole{$ncbi};
  ++$partCount;
}
close (FH);
printf "</table>\n";

endHtml;

printf STDERR "total items: %d\n", $partCount;
