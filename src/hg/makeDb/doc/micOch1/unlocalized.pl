#!/bin/env perl

use strict;
use warnings;

my %accToChr;
my %chrNames;

open (FH, "<../genbank/Primary_Assembly/unlocalized_scaffolds/unlocalized.chr2scaf") or
        die "can not read Primary_Assembly/unlocalized_scaffolds/unlocalized.chr2scaf";
while (my $line = <FH>) {
    next if ($line =~ m/^#/);
    chomp $line;
    my ($chrN, $acc) = split('\s+', $line);
    $acc =~ s/\.1$//;
    $accToChr{$acc} = $chrN;
    $chrNames{$chrN} += 1;
}
close (FH);

foreach my $chrN (keys %chrNames) {
    my $agpFile =  "../genbank/Primary_Assembly/unlocalized_scaffolds/AGP/chr$chrN.unlocalized.scaf.agp.gz";
    my $fastaFile =  "../genbank/Primary_Assembly/unlocalized_scaffolds/FASTA/chr$chrN.unlocalized.scaf.fa.gz";
    open (FH, "zcat $agpFile|") or die "can not read $agpFile";
    open (UC, ">chr${chrN}_random.agp") or die "can not write to chr${chrN}_random.agp";
    while (my $line = <FH>) {
        if ($line =~ m/^#/) {
            print UC $line;
        } else {
            chomp $line;
            my (@a) = split('\t', $line);
            my $acc = $a[0];
            $acc =~ s/\.1//;
            die "ERROR: chrN $chrN not correct for $acc"
                if ($accToChr{$acc} ne $chrN);
            my $ucscName = "chr${chrN}_${acc}_random";
            printf UC "%s", $ucscName;
            for (my $i = 1; $i < scalar(@a); ++$i) {
                printf UC "\t%s", $a[$i];
            }
            printf UC "\n";
        }
    }
    close (FH);
    close (UC);
    printf "chr%s\n", $chrN;
    open (FH, "zcat $fastaFile|") or die "can not read $fastaFile";
    open (UC, ">chr${chrN}_random.fa") or die "can not write to chr${chrN}_random.fa";
    while (my $line = <FH>) {
        if ($line =~ m/^>/) {
            chomp $line;
            my $acc = $line;
            $acc =~ s/.*gb\|//;
            $acc =~ s/\.1. .*//;
            $acc =~ s/\.1//;
            die "ERROR: chrN $chrN not correct for $acc"
                if ($accToChr{$acc} ne $chrN);
            my $ucscName = "chr${chrN}_${acc}_random";
            printf UC ">$ucscName\n";
        } else {
            print UC $line;
        }
    }
    close (FH);
    close (UC);
}
