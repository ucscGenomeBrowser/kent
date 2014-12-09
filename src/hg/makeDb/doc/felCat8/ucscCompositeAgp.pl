#!/bin/env perl

use strict;
use warnings;

sub usage() {
  printf STDERR "usage: ./ucscCompositeAgp.pl ../genbank/GCA_000181335.3_Felis_catus_8.0_assembly_structure/Primary_Assembly\n";
}

my $argc = scalar(@ARGV);

if ($argc != 1) {
  usage;
  exit 255;
}

my $primary = shift(@ARGV);

my %accToChr;

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
    open (FH, "zcat $primary/assembled_chromosomes/AGP/chr${chrN}.comp.agp.gz|") or die "can not read chr${chrN}.comp.agp.gz";
    open (UC, ">chr${chrN}.agp") or die "can not write to chr${chrN}.agp";
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
    open (FH, "zcat $primary/assembled_chromosomes/FASTA/chr${chrN}.fna.gz|") or die "can not read chr${chrN}.fna.gz";
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
