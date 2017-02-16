#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1) {
  printf STDERR "usage: taxonomyNames.pl ../../refseq/assembly_summary_refseq.txt\n";
  printf STDERR "scan the given assembly_summary file to extract the:\n";
  printf STDERR "1. taxonomy ID\n";
  printf STDERR "2. assembly accession\n";
  printf STDERR "3. assembly name\n";
  printf STDERR "4. scientific name\n";
  printf STDERR "5. complete taxonomy string from entrez eutils scanning\n";
  printf STDERR "results accumulate in the file 'allAssemblies.taxonomy.tsv'\n";
  printf STDERR "repeated runs will add to this result file as the assembly_summary file updates\n";
  exit 255;
}

my %taxIdDone;	# key is taxId, value is 1
my $asmSummary = shift;
my %nameProfile;	# key is sciName in the taxonomy lineage, value is count

my $resultFile = "allAssemblies.taxonomy.tsv";
my $newDone = 0;
my $doneLimit = 8000;
open (ID, "<$resultFile") or die "can not read $resultFile";
while (my $line = <ID>) {
  my ($taxId, $rest) = split('\t', $line, 2);
  $taxIdDone{$taxId} = 1;
}
close (ID);

open (ID, ">>$resultFile") or die "can not append to $resultFile";
open (FH, "grep -v '^#' $asmSummary | cut -f1,7,8,16|") or die "can not grep $asmSummary";
while (($newDone < $doneLimit) && (my $line = <FH>)) {
  chomp $line;
  my ($asmAcc, $taxId, $sciName, $asmName) = split('\t', $line);
  next if exists($taxIdDone{$taxId});
  $asmName =~ s/ /_/g;
  my $taxonomyString = "";
  printf "#\t%7d\t%s\t%s\t%s\n", $taxId, $asmAcc, $sciName, $asmName;
  open (TX, "wget -O /dev/stdout 'https://eutils.ncbi.nlm.nih.gov/entrez/eutils/efetch.fcgi?db=taxonomy&id=$taxId&retmode=xml' 2> /dev/null | grep -i ScientificName | headRest 1 stdin|") or die "can not wget entrez/eutils";
  while (my $sciName = <TX>) {
    chomp $sciName;
    $sciName =~ s/.*<ScientificName>//;
    $sciName =~ s#</ScientificName>##;
#    printf "%s\n", $sciName;
    $taxonomyString .= sprintf("%s;", $sciName);
    $nameProfile{$sciName} += 1;
  }
  close (TX);
  $taxIdDone{$taxId} = 1;
  printf ID "%d\t%s\t%s\t%s\t%s\n", $taxId, $asmAcc, $asmName, $sciName, $taxonomyString;
  ++$newDone;
}
close (FH);
close (ID);

foreach my $sciName (keys %nameProfile) {
  printf "%d\t%s\n", $nameProfile{$sciName}, $sciName;
}
__END__

https://eutils.ncbi.nlm.nih.gov/entrez/eutils/efetch.fcgi?db=taxonomy&id=9660&retmode=xml

 wget -O /dev/stdout "https://eutils.ncbi.nlm.nih.gov/entrez/eutils/efetch.fcgi?db=taxonomy&id=9660&retmode=xml" | grep -i ScientificName

# assembly_accession    bioproject      biosample       wgs_master      refseq_category taxid   species_taxid   organism_name   infraspecific_name      isolate version_status  assembly_level  release_type    genome_rep      seq_rel_date    asm_name        submitter       gbrs_paired_asm paired_asm_comp ftp_path        excluded_from_refseq
GCF_000001215.4 PRJNA164        SAMN02803731            reference genome        7227    7227    Drosophila melanogaster                 latest  Chromosome      Major   Full    2014/08/01      Release 6 plus ISO1 MT  The FlyBase Consortium/Berkeley Drosophila Genome Project/Celera Genomics       GCA_000001215.4 identical       ftp://ftp.ncbi.nlm.nih.gov/genomes/all/GCF/000/001/215/GCF_000001215.4_Release_6_plus_ISO1_MT   

