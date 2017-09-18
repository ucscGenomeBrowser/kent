#!/usr/bin/env perl

use strict;
use warnings;

# adjust values here for new patch level
my $patchLevel = 9;
my $asmId = "GCA_000001405.24_GRCh38.p${patchLevel}";
my $patchOrdinal = "ninth";


my $haploCount = `cat hg38Patch${patchLevel}Haplotypes.bed | wc -l`;
chomp $haploCount;
my $patchCount = `cat hg38Patch${patchLevel}Patches.bed | wc -l`;
chomp $patchCount;

sub beginHtml() {

printf "<h2>Description</h2>\n";

printf "<p>
   Patch release $patchLevel is the $patchOrdinal patch release for the GRCh38 assembly.
   Patch release $patchLevel includes all contents that were in patch release %d.
   This is a minor release of GRCh38 that does not disrupt the coordinate
   system in the reference sequence GRCh38.
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
   <a href=\"ftp://ftp.ncbi.nlm.nih.gov/genomes/genbank/vertebrate_mammalian/Homo_sapiens/all_assembly_versions/${asmId}/\" target=\"_blank\">FTP</a>.
</p>\n";
}

my %seqName;
my %seqRole;
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
printf "<tr><th>GRCh38 reference<br>location</th><th>NCBI Entrez<br>nucleotide record</th><th>GRC region<br>name</th><th>sequence<br>name</th><th>patch<br>type</th></tr>\n";
open (FH, "sort -k1,1 -k2,2n patchLocations.bed|") or die "can not read patchLocations.bed";
while (my $line = <FH>) {
  chomp $line;
  my ($chr, $start, $end, $name) = split('\t', $line);
  my $ncbi = $name;
  $ncbi =~ s/_fix|_alt//;
  $ncbi =~ s/chr.*_//;
  $ncbi =~ s/v/./;
  printf "<tr><td><a href=\"../cgi-bin/hgTracks?db=hg38&position=%s:%d-%d\">%s:%d-%d</a></td>\n", $chr, $start+1, $end, $chr, $start+1, $end;
  printf "    <td><a href=\"https://www.ncbi.nlm.nih.gov/nuccore/%s\" target=\"_blank\">%s</a></td>\n", $ncbi, $name;
  printf "    <td><a href=\"https://www.ncbi.nlm.nih.gov/projects/genome/assembly/grc/region.cgi?name=%s&asm=GRCh38.p${patchLevel}\" target=\"_blank\">%s</a></td>\n", $region{$ncbi}, $region{$ncbi};
  printf "    <td>%s</td><td>%s</td>\n</tr>\n", $seqName{$ncbi}, $seqRole{$ncbi};
  ++$partCount;
}
close (FH);
printf "</table>\n";

endHtml;

printf STDERR "total items: %d\n", $partCount;

__END__
https://www.ncbi.nlm.nih.gov/projects/genome/assembly/grc/region.cgi?name=REGION108&asm=GRCh38.p2

https://www.ncbi.nlm.nih.gov/nuccore/$$

# Sequence-Name	Sequence-Role	Assigned-Molecule	Assigned-Molecule-Location/Type	GenBank-Accn	Relationship	RefSeq-Accn	Assembly-Unit
1	assembled-molecule	1	Chromosome	CM000663.2	=	NC_000001.11	Primary Assembly
2	assembled-molecule	2	Chromosome	CM000664.2	=	NC_000002.12	Primary Assembly

HG986_PATCH	fix-patch	1	Chromosome	KN196472.1	=	NW_009646194.1	PATCHES
HG2058_PATCH	fix-patch	1	Chromosome	KN196473.1	=	NW_009646195.1	PATCHES
HG2104_PATCH	fix-patch	1	Chromosome	KN196474.1	=	NW_009646196.1	PATCHES

HSCHR19KIR_ABC08_AB_HAP_C_P_CTG3_1	alt-scaffold	19	Chromosome	KI270918.1	=	NT_187672.1	ALT_REF_LOCI_24
HSCHR19KIR_ABC08_AB_HAP_T_P_CTG3_1	alt-scaffold	19	Chromosome	KI270919.1	=	NT_187673.1	ALT_REF_LOCI_25
HSCHR19KIR_FH05_A_HAP_CTG3_1	alt-scaffold	19	Chromosome	KI270920.1	=	NT_187674.1	ALT_REF_LOCI_26

# Region-Name   Chromosome      Chromosome-Start        Chromosome-Stop Scaffold-Role   Scaffold-GenBank-Accn   Scaffold-RefSeq-Accn    Assembly-Unit
REGION108       1       2448811 2791270 alt-scaffold    KI270762.1      NT_187515.1     ALT_REF_LOCI_1
PRAME_REGION_1  1       13075113        13312803        alt-scaffold    KI270766.1      NT_187517.1     ALT_REF_LOCI_1
REGION200       1       17157487        17460319        fix-patch       KN538361.1      na      PATCHES

chr1	17157487	17460319	chr1_KN538361v1_fix
chr1	26512383	26678582	chr1_KN196473v1_fix
chr1	41250328	41436604	chr1_KN196472v1_fix
chr1	112909422	113029606	chr1_KN196474v1_fix
chr1	210220259	210677001	chr1_KN538360v1_fix
chr2	233054663	233420161	chr2_KN538363v1_fix
chr2	239696904	239905046	chr2_KN538362v1_fix
chr3	44474647	44925818	chr3_KN196475v1_fix
chr3	72325441	72740399	chr3_KN538364v1_fix
chr3	91247723	91553319	chr3_KN196476v1_fix
chr5	21098347	21230960	chr5_KN196477v1_alt
chr6	67533466	67796090	chr6_KN196478v1_fix
chr9	133174055	133504070	chr9_KN196479v1_fix
chr10	38918270	39338430	chr10_KN538367v1_fix
chr10	74797382	75073121	chr10_KN196480v1_fix
chr10	124109957	124195365	chr10_KN538366v1_fix
chr10	131588434	131607480	chr10_KN538365v1_fix
chr11	7779133	7972558	chr11_KN538368v1_alt
chr11	118978401	119087276	chr11_KN196481v1_fix
chr12	12031003	12572035	chr12_KN538369v1_fix
chr12	17717409	17928910	chr12_KN196482v1_fix
chr12	122057735	122145576	chr12_KN538370v1_fix
chr13	18171249	18358106	chr13_KN538372v1_fix
chr13	86269523	86292877	chr13_KN196483v1_fix
chr13	100900735	101049613	chr13_KN538373v1_fix
chr13	113936976	114147229	chr13_KN538371v1_fix
chr19	39740125	40095774	chr19_KN196484v1_fix
chr22	42077656	42253758	chr22_KN196485v1_alt
chr22	42077656	42253758	chr22_KN196486v1_alt
chrY	56821510	56887902	chrY_KN196487v1_fix
chr15_KI270905v1_alt	1	5161414	chr15_KN538374v1_fix
