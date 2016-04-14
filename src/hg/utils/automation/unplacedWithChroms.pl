#!/usr/bin/env perl

use strict;
use warnings;

sub usage() {
  printf STDERR "usage: ./unplacedWithChroms.pl ../refseq/*_assembly_structure/Primary_Assembly\n";
}

my $argc = scalar(@ARGV);

if ($argc != 1) {
  usage;
  exit 255;
}

my $primary = shift(@ARGV);
my %ucscToNcbiName;   # key is ucscName, value is ncbi name

my $agpFile =  "$primary/unplaced_scaffolds/AGP/unplaced.scaf.agp.gz";
open (FH, "zcat $agpFile|") or die "can not read $agpFile";
open (UC, ">chrUn.agp") or die "can not write to chrUn.agp";
while (my $line = <FH>) {
    if ($line =~ m/^#/) {
        print UC $line;
    } else {
        chomp $line;
        my $ncbiName = $line;
        $ncbiName =~ s/\s.*//;
        my $ucscName = "chrUn_$ncbiName";
        $ucscName =~ s/\./v/;
        $line =~ s/\./v/;
        printf UC "chrUn_%s\n", $line;
        $ucscToNcbiName{$ucscName} = $ncbiName;
    }
}
close (FH);
close (UC);

my $sequenceCount = 0;
open (FA, "|gzip -c > chrUn.fa.gz") or die "can not write to chrUn.fa.gz";
foreach my $ucscName (sort keys %ucscToNcbiName) {
  my $ncbiName = $ucscToNcbiName{$ucscName};
  printf FA ">%s\n", $ucscName;
  print FA `twoBitToFa -noMask refseq.2bit:$ncbiName stdout | grep -v "^>"`;
  ++$sequenceCount;
}
close (FA);

printf STDERR "# processed %d sequences into chrUn.fa.gz\n", $sequenceCount;
