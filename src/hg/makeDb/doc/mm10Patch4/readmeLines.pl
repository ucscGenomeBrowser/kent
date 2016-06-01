#!/bin/env perl
use strict;
use warnings;

my %regions = ();
my %reports = ();
my %chromLocation = ();
my %chromStarts = ();
my %chromEnds = ();

if (scalar(@ARGV) != 4)
{
  print STDERR "Usage: readmeLines.pl reportFile regionsFile patchLocations.bed new.sequences.list\n";
  exit 0;
}

my $reportFile = shift;
my $regionsFile = shift;
my $locationsFile = shift;
my $newSequencesFile = shift;

open REPORT, $reportFile or die "Can't open report file $reportFile";
while (my $line = <REPORT>)
{
  next if $line =~ /^\s*#/; # skip comments
  next if $line =~ /^\s*$/; # skip empty lines, not that I expect any
  chomp($line);
  my @fields = split /\s+/, $line;
  # fifth field has the genbank accession
  # first field has sequence name
  $reports{$fields[4]} = $fields[0];
}
close REPORT;

open REGIONS, $regionsFile or die "Can't open regions file $regionsFile";
while (my $line = <REGIONS>)
{
  next if $line =~ /^\s*#/; # skip comments
  next if $line =~ /^\s*$/; # skip empty lines, not that I expect any
  chomp($line);
  my @fields = split /\s+/, $line;
  next if $fields[5] eq "na";
  # sixth field contains the accession
  # first is region name, fifth is patch type
  $regions{$fields[5]} = join " ", ($fields[0], $reports{$fields[5]}, $fields[4]);
  $chromStarts{$fields[5]} = $fields[2];
  $chromEnds{$fields[5]} = $fields[3];
}
close(REGIONS);

open LOCATIONS, $locationsFile;
while (my $line = <LOCATIONS>)
{
  chomp $line;
  my @fields = split /\s+/, $line;
  $chromLocation{$fields[3]} = $fields[0]; # actual chrom indexed by ucsc name for patch
}
close LOCATIONS;

open NEWLIST, $newSequencesFile;
while (my $line = <NEWLIST>)
{
  chomp $line;
  my @fields = split /\s+/, $line;
  my $ucscName = $fields[0];
  my $accession = $fields[2];
  my $chrom = $chromLocation{$ucscName};
  my $chromStart =  $chromStarts{$accession}; # not bed coordinate style
  my $chromEnd =  $chromEnds{$accession};
  print "$chrom:$chromStart-$chromEnd $ucscName " . $regions{$accession} . "\n";
}
close NEWLIST;

