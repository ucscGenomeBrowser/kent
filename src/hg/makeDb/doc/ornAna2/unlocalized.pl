#!/bin/env perl

use strict;
use warnings;

sub usage() {
  printf STDERR "usage: ./unlocalized.pl ../genbank/GCA_000181335.3_Felis_catus_8.0_assembly_structure/Primary_Assembly\n";
}

my $argc = scalar(@ARGV);

if ($argc != 1) {
  usage;
  exit 255;
}

my $primary = shift(@ARGV);

my %accToChr;
my %chrNames;

open (FH, "<$primary/unlocalized_scaffolds/unlocalized.chr2scaf") or
        die "can not read Primary_Assembly/unlocalized_scaffolds/unlocalized.chr2scaf";
while (my $line = <FH>) {
    next if ($line =~ m/^#/);
    chomp $line;
    my ($chrN, $acc) = split('\s+', $line);
    $acc =~ s/\./v/;
    $accToChr{$acc} = $chrN;
    $chrNames{$chrN} += 1;
}
close (FH);

foreach my $chrN (keys %chrNames) {
    my $agpFile =  "$primary/unlocalized_scaffolds/AGP/chr$chrN.unlocalized.scaf.agp.gz";
    my $fastaFile =  "$primary/unlocalized_scaffolds/FASTA/chr$chrN.unlocalized.scaf.fna.gz";
    open (FH, "zcat $agpFile|") or die "can not read $agpFile";
    open (UC, ">chr${chrN}_random.agp") or die "can not write to chr${chrN}_random.agp";
    while (my $line = <FH>) {
        if ($line =~ m/^#/) {
            print UC $line;
        } else {
            chomp $line;
            my (@a) = split('\t', $line);
            my $acc = $a[0];
            $acc =~ s/\./v/;
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
            $acc =~ s/ .*//;
            $acc =~ s/\./v/;
            $acc =~ s/>//;
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
