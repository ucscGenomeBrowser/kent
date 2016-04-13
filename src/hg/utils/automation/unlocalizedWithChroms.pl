#!/usr/bin/env perl

use strict;
use warnings;

sub usage() {
  printf STDERR "usage: ./unlocalizedWithChroms.pl ../refseq/*_assembly_structure/Primary_Assembly\n";
}

my $argc = scalar(@ARGV);

if ($argc != 1) {
  usage;
  exit 255;
}

my $primary = shift(@ARGV);

my %accToChr;		# key is NCBI accession, value is chrom number
my %accToUcscName;	# key is NCBI accession, value is UCSC name
my %chrNames;		# key is chrom number, value array of NCBI accession ids

open (FH, "<$primary/unlocalized_scaffolds/unlocalized.chr2scaf") or
        die "can not read Primary_Assembly/unlocalized_scaffolds/unlocalized.chr2scaf";
while (my $line = <FH>) {
    next if ($line =~ m/^#/);
    chomp $line;
    my ($chrN, $ncbiAcc) = split('\s+', $line);
    $accToChr{$ncbiAcc} = $chrN;
    if (! exists($chrNames{$chrN})) {
      my @accessions;
      $chrNames{$chrN} = \@accessions;
    }
    my $chrNptr = $chrNames{$chrN};
    push @$chrNptr, $ncbiAcc;
    my $acc = $ncbiAcc;
    $acc =~ s/\./v/;
    my $ucscName = "chr${chrN}_${acc}_random";
    $accToUcscName{$ncbiAcc} = $ucscName;
}
close (FH);

my $sequenceCount = 0;
my $chrCount = 0;
foreach my $chrN (keys %chrNames) {
    ++$chrCount;
    my $agpFile =  "$primary/unlocalized_scaffolds/AGP/chr$chrN.unlocalized.scaf.agp.gz";
    open (FH, "zcat $agpFile|") or die "can not read $agpFile";
    open (UC, ">chr${chrN}_random.agp") or die "can not write to chr${chrN}_random.agp";
    while (my $line = <FH>) {
        if ($line =~ m/^#/) {
            print UC $line;
        } else {
            chomp $line;
            my (@a) = split('\t', $line);
            my $acc = $a[0];
            my $ucscName = $accToUcscName{$acc};
            die "ERROR: chrN $chrN not correct for $acc"
                if ($accToChr{$acc} ne $chrN);
            printf UC "%s", $ucscName;
            for (my $i = 1; $i < scalar(@a); ++$i) {
                printf UC "\t%s", $a[$i];
            }
            printf UC "\n";
        }
    }
    close (FH);
    close (UC);
    open (FA, "|gzip -c > chr${chrN}_random.fa.gz") or die "can not write to chr${chrN}_random.fa.gz";
    foreach my $chrNptr ($chrNames{$chrN}) {
       foreach my $ncbiAcc (@$chrNptr) {
         printf FA ">%s\n", $accToUcscName{$ncbiAcc};
       print FA `twoBitToFa -noMask refseq.2bit:$ncbiAcc stdout | grep -v "^>"`;
         ++$sequenceCount;
       }
    }
    close (FA);
    printf STDERR "# %s\n", $chrN;
}
printf STDERR "# processed %d sequences into chr*_random.gz %d files\n",
   $sequenceCount, $chrCount;
