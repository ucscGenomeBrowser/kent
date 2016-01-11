#!/usr/bin/env perl

use strict;
use warnings;

sub usage() {
  printf STDERR "usage: ./unlocalizedAgp.pl [ucsc|ncbi] <pathTo>/*_assembly_structure/Primary_Assembly\n";
  printf STDERR "output to stdout, redirect to > unlocalized.agp\n";
  printf STDERR "reroute stderr output thru a 'sort -u' to write a file\n";
  printf STDERR "    of the UCSC to NCBI name correspondence, e.g.:\n";
  printf STDERR "       2> >(sort -u > xxx.ucsc.to.ncbi.unlocalized.names)\n";
  printf STDERR "expecting to find unlocalized_scaffolds hierarchy at given path\n";
  printf STDERR "the ucsc|ncbi selects the naming scheme.\n";
}

my $argc = scalar(@ARGV);

if ($argc != 2) {
  usage;
  exit 255;
}

my $ncbiUcsc = shift(@ARGV);
my $primary = shift(@ARGV);

my %accToChr;
my %chrNames;

open (FH, "<$primary/unlocalized_scaffolds/unlocalized.chr2scaf") or
        die "can not read $primary/unlocalized_scaffolds/unlocalized.chr2scaf";
while (my $line = <FH>) {
    next if ($line =~ m/^#/);
    chomp $line;
    my ($chrN, $ncbiAcc) = split('\s+', $line);
    my $acc = $ncbiAcc;
    $acc =~ s/\./v/ if ($ncbiUcsc ne "ncbi");
    $accToChr{$acc} = $chrN;
    $chrNames{$chrN} += 1;
}
close (FH);

foreach my $chrN (keys %chrNames) {
    my $agpFile =  "$primary/unlocalized_scaffolds/AGP/chr$chrN.unlocalized.scaf.agp.gz";
    open (FH, "zcat $agpFile|") or die "can not read $agpFile";
    while (my $line = <FH>) {
        if ($line !~ m/^#/) {
            chomp $line;
            my (@a) = split('\t', $line);
            my $ncbiAcc = $a[0];
            my $acc = $ncbiAcc;
            $acc =~ s/\./v/ if ($ncbiUcsc ne "ncbi");
            die "ERROR: chrN $chrN not correct for $acc"
                if ($accToChr{$acc} ne $chrN);
            my $ucscName = "chr${chrN}_${acc}_random";
            $ucscName = "${acc}" if ($ncbiUcsc eq "ncbi");;
            printf STDERR "%s\t%s\n", $ucscName, $ncbiAcc if ($ncbiUcsc ne "ncbi");
            printf "%s", $ucscName;
            for (my $i = 1; $i < scalar(@a); ++$i) {
                printf "\t%s", $a[$i];
            }
            printf "\n";
        }
    }
    close (FH);
}
