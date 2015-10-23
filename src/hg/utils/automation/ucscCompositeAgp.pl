#!/bin/env perl

use strict;
use warnings;

sub usage() {
  printf STDERR "usage: ./ucscCompositeAgp.pl ../genbank/*_assembly_structure/Primary_Assembly\n";
}

my $argc = scalar(@ARGV);

if ($argc != 1) {
  usage;
  exit 255;
}

my $primary = shift(@ARGV);

my %accToChr;
# #Chromosome     Accession.version
# 1       CM002885.1
# 2       CM002886.1
# 3       CM002887.1

open (FH, "<$primary/assembled_chromosomes/chr2acc") or
        die "can not read Primary_Assembly/assembled_chromosomes/chr2acc";
while (my $line = <FH>) {
    next if ($line =~ m/^#/);
    chomp $line;
    my ($chrN, $acc) = split('\s+', $line);
    $accToChr{$acc} = $chrN;
}
close (FH);

foreach my $acc (keys %accToChr) {
    my $chrN =  $accToChr{$acc};
    print "$acc $accToChr{$acc}\n";
    my $theirChr = "chr${chrN}";
    open (FH, "zcat $primary/assembled_chromosomes/AGP/${theirChr}.comp.agp.gz|") or die "can not read chr${chrN}.comp.agp.gz";
    open (UC, ">chr${chrN}.agp") or die "can not write to ${theirChr}.agp";
    while (my $line = <FH>) {
        if ($line =~ m/^#/) {
            print UC $line;
        } else {
            $line =~ s/^$acc/chr${chrN}/;
            print UC $line;
        }
    }
    close (FH);
    close (UC);
    open (FH, "zcat $primary/assembled_chromosomes/FASTA/${theirChr}.fna.gz|") or die "can not read chr${chrN}.fna.gz";
    open (UC, ">chr${chrN}.fa") or die "can not write to chr${chrN}.fa";
    while (my $line = <FH>) {
        if ($line =~ m/^>/) {
            printf UC ">chr${chrN}\n";
        } else {
            print UC $line;
        }
    }
    close (FH);
    close (UC);
}
