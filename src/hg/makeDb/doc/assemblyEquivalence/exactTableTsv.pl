#!/usr/bin/env perl

use strict;
use warnings;

open (FH, "ls *.exact.txt|") or die "can not ls *.exact.txt";
while (my $line = <FH>) {
  chomp $line;
  my ($sourceAuthority, $destinationAuthority, undef) = split('\.', $line, 3);
  open (EX, "<$line") or die "can not read $line";
  while (my $equiv = <EX>) {
    chomp $equiv;
    my ($source, $sourceCount, undef, $destination, $destinationCount, undef) =
       split('\s+', $equiv);
    # in the case of these exact matches, the counts are all the same
    if ($sourceCount != $destinationCount) {
      printf "ERROR: exact matches expect source=destination counts: %d != %d\n", $sourceCount, $destinationCount;
      exit 255;
    }
    printf "%s\t%s\t%s\t%s\t%d\t%d\t%d\n", $source, $destination,
        $sourceAuthority, $destinationAuthority, $sourceCount, $sourceCount, $destinationCount;
  }
  close (EX);
}
close (FH);

__END__

ensembl.genbank.exact.txt
Acanthochromis_polyacanthus.ASM210954v1	30414	30414	GCA_002109545.1_ASM210954v1	30414	30414
ensembl.refseq.exact.txt
ensembl.ucscDb.exact.txt

genbank.ensembl.exact.txt
genbank.refseq.exact.txt
genbank.ucscDb.exact.txt

refseq.ensembl.exact.txt
refseq.genbank.exact.txt
refseq.ucscDb.exact.txt

ucscDb.ensembl.exact.txt
ucscDb.genbank.exact.txt
ucscDb.refseq.exact.txt
table asmEquivalent
"Equivalence relationship of assembly versions, Ensembl: UCSC, NCBI genbank/refseq"
    (
    string source;          "assembly name"
    string destination;     "equivalent assembly name"
    enum ("ensembl", "ucsc", "genbank", "refseq") sourceAuthority; "origin of source assembly"
    enum ("ensembl", "ucsc", "genbank", "refseq") destinationAuthority; "origin of equivalent assembly"
    uint   matchCount;       "number of exactly matching sequences"
    uint   sourceCount;      "number of sequences in source assembly"
    uint   destinationCount; "number of sequences in equivalent assembly"
    )

