#!/usr/bin/env perl

use strict;
use warnings;

sub usage() {
  printf STDERR "usage: regionScan.pl <extract.new.list> <genbank/GCA_000001405.17_GRCh38.p2_assembly_regions.txt>\n";
}

my $argc = scalar(@ARGV);
if ($argc != 2) {
  usage;
  exit 255;
}

my %ncbiToUcsc;  # key is NCBI name, value is UCSC name
# construct name translation table for chromosome names
open (FH, "hgsql -N -e 'select chrom,name from scaffolds;' hg38|") or die "can not select from hg38.scaffolds";
while (my $line = <FH>) {
  chomp $line;
  my ($ucsc, $ncbi) = split('\s+', $line);
  $ncbiToUcsc{$ncbi} = $ucsc;
}
close (FH);

my $newList = shift;
my $asmRegions = shift;

my %newList;
open (FH, "<$newList") or die "can not read $newList";
while (my $seqName = <FH>) {
   chomp $seqName;
   $newList{$seqName} = 1; 
}
close (FH);

open (FH, "<$asmRegions") or die "can not read $asmRegions";
while (my $line = <FH>) {
  next if ($line =~ m/^#/);
  chomp $line;
  my ($regionName, $ncbiName, $chrStart, $chrEnd, $role, $genbankAcc, $refSeqAcc, $unit) = split('\t', $line);
  if (exists($newList{$genbankAcc})) {
    my $extn = "xxx";
    if ($role eq "fix-patch") {
      $extn = "fix";
    } elsif ($role eq "novel-patch") {
      $extn = "alt";
    } else {
      die "do not recognize role: $role";
    }
    die "can not find ncbi to ucsc name translation for '$ncbiName'" if (! exists($ncbiToUcsc{$ncbiName}));
    my $chrName = $ncbiToUcsc{$ncbiName};
    my $newUcscName = $chrName;
    $newUcscName =~ s/_.*//;
    my $ucscName = $genbankAcc;
    $ucscName =~ s/\./v/;
    printf "%s\t%d\t%d\t%s_%s_%s\n", $chrName, $chrStart, $chrEnd, $newUcscName, $ucscName, $extn;
  }
}
close (FH);

__END__

# Region-Name	Chromosome	Chromosome-Start	Chromosome-Stop	Scaffold-Role	Scaffold-GenBank-Accn	Scaffold-RefSeq-Accn	Assembly-Unit
REGION108	1	2448811	2791270	alt-scaffold	KI270762.1	NT_187515.1	ALT_REF_LOCI_1
PRAME_REGION_1	1	13075113	13312803	alt-scaffold	KI270766.1	NT_187517.1	ALT_REF_LOCI_1
REGION194	6	67533466	67796090	fix-patch	KN196478.1	NW_009646200.1	PATCHES
REGION193	5	21098347	21230960	novel-patch	KN196477.1	NW_009646199.1	PATCHES
REGION198	Y	56821510	56887902	fix-patch	KN196487.1	NW_009646209.1	PATCHES
REGION1A	HSCHR15_4_CTG8	1	5161414	fix-patch	KN538374.1	na	PATCHES
