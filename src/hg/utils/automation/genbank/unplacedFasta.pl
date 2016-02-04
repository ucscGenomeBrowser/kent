#!/usr/bin/env perl

use strict;
use warnings;
use File::Temp qw(tempfile);

sub usage() {
  printf STDERR "usage: ./unplacedFasta.pl [ucsc|ncbi] <chrUn_> <pathTo>/*_assembly_structure/Primary_Assembly/unplaced_scaffolds/AGP/unplaced.scaf.agp.gz <pathTo>/*.ncbi.2bit\n";
  printf STDERR "    output is to stdout, redirect to a file > unplaced.fa\n";
  printf STDERR "    the first argument is the chrUn_ prefix to add to the\n";
  printf STDERR "    contig names, can be empty string quoted \"\" to have\n";
  printf STDERR "    no prefix.\n";
  printf STDERR "the ucsc|ncbi selects the naming scheme\n";
}

my $argc = scalar(@ARGV);

if ($argc != 4) {
  usage;
  exit 255;
}

my $ncbiUcsc = shift(@ARGV);
my $chrPrefix = shift(@ARGV);
my $agpFile = shift(@ARGV);
my $twoBitFile = shift(@ARGV);
my %contigName;   # key is NCBI contig name, value is UCSC contig name

open (FH, "zcat $agpFile|") or die "can not read $agpFile";
while (my $line = <FH>) {
    if ($line !~ m/^#/) {
        my ($ctgName, $rest) = split('\s+', $line, 2);
        my $ucscName = $ctgName;
        $ucscName =~ s/\./v/ if ($ncbiUcsc ne "ncbi");
        $contigName{$ctgName} = $ucscName;
    }
}
close (FH);

my ($fh, $tmpFile) = tempfile("seqList_XXXX", DIR=>'/dev/shm', SUFFIX=>'.txt', UNLINK=>1);
foreach my $ctgName (sort keys %contigName) {
   printf $fh "%s\n", $ctgName;
}
close $fh;

open (FH, "twoBitToFa -seqList=$tmpFile $twoBitFile stdout|") or die "can not twoBitToFa $twoBitFile";
while (my $line = <FH>) {
   if ($line =~ m/^>/) {
      chomp $line;
      $line =~ s/^>//;
      $line =~ s/ .*//;
      die "ERROR: twoBitToFa $twoBitFile returns unknown acc $line" if (! exists($contigName{$line}));
      printf ">%s%s\n", $chrPrefix, $contigName{$line};
   } else {
      print $line;
   }
}
close(FH);
