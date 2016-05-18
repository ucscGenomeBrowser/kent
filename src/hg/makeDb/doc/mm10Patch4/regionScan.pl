#!/usr/bin/env perl

use strict;
use warnings;

sub usage() {
  printf STDERR "usage: regionScan.pl <extract.new.list> ucsc.refseq.tab <refseq/GCF_000001635.24_GRCm38.p4_assembly_regions.txt>\n";
}

my $argc = scalar(@ARGV);
if ($argc != 3) {
  usage;
  exit 255;
}

my $newList = shift;
my $ucscRefseq = shift;
my $asmRegions = shift;


my %ncbiToUcsc;  # key is NCBI refseq name, value is UCSC name
# construct name translation table for chromosome names
open (FH, "<$ucscRefseq") or die "can not read $ucscRefseq";
while (my $line = <FH>) {
  chomp $line;
  my ($ucsc, $ncbi) = split('\s+', $line);
  $ncbiToUcsc{$ncbi} = $ucsc;
}
close (FH);

my %newList;
open (FH, "<$newList") or die "can not read $newList";
while (my $seqName = <FH>) {
   chomp $seqName;
   $newList{$seqName} = 1; 
}
close (FH);
my %doneList;

open (FH, "<$asmRegions") or die "can not read $asmRegions";
while (my $line = <FH>) {
  next if ($line =~ m/^#/);
  chomp $line;
  my ($regionName, $ncbiName, $chrStart, $chrEnd, $role, $genbankAcc, $refSeqAcc, $unit) = split('\t', $line);
  if (exists($newList{$genbankAcc})) {
    $doneList{$genbankAcc} = 1;
    my $extn = "xxx";
    if ($role eq "fix-patch") {
      $extn = "fix";
    } elsif ($role eq "alt-scaffold") {
      $extn = "alt";
    } elsif ($role eq "novel-patch") {
      $extn = "alt";
    } else {
      die "do not recognize role: $role";
    }
    die "can not find ncbi to ucsc name translation for '$ncbiName'" if (! exists($ncbiToUcsc{$genbankAcc}));
    my $chrName = $ncbiToUcsc{$genbankAcc};
    my $newUcscName = $chrName;
    $newUcscName =~ s/_.*//;
    my $ucscName = $genbankAcc;
    $ucscName =~ s/\.1//;
    $ucscName =~ s/\./v/;
    printf "chr%s\t%d\t%d\t%s_%s_%s\n", $ncbiName, $chrStart-1, $chrEnd, $newUcscName, $ucscName, $extn;
#     printf "%s\t%d\t%d\t%s_%s_%s\n", $chrName, $chrStart-1, $chrEnd, $newUcscName, $ucscName, $extn;
  } else {
    printf STDERR "# not finding $regionName $genbankAcc on the newList\n";
  }
}
close (FH);

foreach my $genbankAcc (sort keys %newList) {
  printf STDERR "# not done $genbankAcc\n" if (! exists($doneList{$genbankAcc}));
}

__END__

# Region-Name	Chromosome	Chromosome-Start	Chromosome-Stop	Scaffold-Role	Scaffold-GenBank-Accn	Scaffold-RefSeq-Accn	Assembly-Unit
REGION108	1	2448811	2791270	alt-scaffold	KI270762.1	NT_187515.1	ALT_REF_LOCI_1
PRAME_REGION_1	1	13075113	13312803	alt-scaffold	KI270766.1	NT_187517.1	ALT_REF_LOCI_1
REGION194	6	67533466	67796090	fix-patch	KN196478.1	NW_009646200.1	PATCHES
REGION193	5	21098347	21230960	novel-patch	KN196477.1	NW_009646199.1	PATCHES
REGION198	Y	56821510	56887902	fix-patch	KN196487.1	NW_009646209.1	PATCHES
REGION1A	HSCHR15_4_CTG8	1	5161414	fix-patch	KN538374.1	na	PATCHES
