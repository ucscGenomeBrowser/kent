#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc != 1) {
  printf STDERR "usage: ./parseGff.pl <GRCh37.p13_issues.gff>\n";
  exit 255;
}

my $gffFile = shift;
open (FH, "zcat $gffFile|") or die "can not read $gffFile";
while (my $line = <FH>) {
  next if ($line =~ m/^#/);
  chomp $line;
  my ($seqId, $source, $type, $start, $end, $score, $strand, $phase, $attributes) = split('\s+', $line, 9);
  my @attribs = split(';', $attributes);
  my $attribCount = scalar(@attribs);
  my %attribs;
  for (my $i = 0; $i < $attribCount; ++$i) {
     my ($name, $tag) = split('=', $attribs[$i]);
     $attribs{$name} = $tag;
  }
  die "no chr defined for $seqId" if (!defined($attribs{'chr'}));
  die "no Name defined for $seqId" if (!defined($attribs{'Name'}));
  die "no status defined for $seqId" if (!defined($attribs{'status'}));
  die "no type defined for $seqId" if (!defined($attribs{'type'}));
  my $bed5 = sprintf("<BR><B>Category: </B>%s<BR><B>Status: </B>%s</B><BR>", $attribs{'type'}, $attribs{'status'});
  $bed5 =~ s/ /&nbsp;/g;
#  if ("Un" eq $attribs{'chr'}) {
    printf "%s\t%d\t%d\t%s\t%s\n", $seqId, $start-1, $end, $attribs{'Name'}, $bed5;
#  } else {
#    printf "chr%s\t%d\t%d\t%s\t%s\n", $attribs{'chr'}, $start-1, $end, $attribs{'Name'}, $bed5;
#  }
}
close (FH);

__END__

NC_000010.11    GRC     region  133740467       133777437       .       +      .Name=HG-155;type=Unknown;chr=10;status=Resolved
NT_187369.1     GRC     region  1       41717   .       +       .       Name=HG-1908;type=GRC Housekeeping;chr=1;status=Resolved
NC_000018.10    GRC     region  71004235        71150220        .       +      .Name=HG-966;type=Clone Problem;chr=18;status=Stalled

