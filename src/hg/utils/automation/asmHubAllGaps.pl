#!/usr/bin/env perl

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 5) {
  printf STDERR "usage: asmHubAllGaps.pl asmId asmId.names.tab asmId.agp.gz hubUrl bbi/asmId > asmId.allGaps.html\n";
  printf STDERR "where asmId is the assembly identifier,\n";
  printf STDERR "and   asmId.names.tab is naming file for this assembly,\n";
  printf STDERR "and   asmId.agp.gz is AGP file for this assembly,\n";
  printf STDERR "and   hubUrl is the URL to this assembly directory for the AGP file.\n";
  printf STDERR "and   bbi/asmId is the path prefix to .allGaps.bb.\n";
  exit 255;
}

my $asmId = shift;
my $namesFile = shift;
my $agpFile = shift;
my $agpFileBase = basename($agpFile);
my $hubUrl = shift;
my $bbiPrefix = shift;
my $allGapsBbi = "$bbiPrefix.allGaps.bb";

if ( ! -s $agpFile ) {
  printf STDERR "ERROR: can not find AGP file:\n\t'%s'\n", $agpFile;
  exit 255;
}
if ( ! -s $allGapsBbi ) {
  printf STDERR "ERROR: can not find allGaps bbi file:\n\t'%s'\n", $allGapsBbi;
  exit 255;
}

my $em = "<em>";
my $noEm = "</em>";
my $assemblyDate = `grep -v "^#" $namesFile | cut -f9`;
chomp $assemblyDate;
my $ncbiAssemblyId = `grep -v "^#" $namesFile | cut -f10`;
chomp $ncbiAssemblyId;
my $organism = `grep -v "^#" $namesFile | cut -f5`;
chomp $organism;

print <<_EOF_
<h2>Description</h2>
<p>
This track shows all gaps (any sequence of N's) in the $assemblyDate $em${organism}$noEm/$asmId genome assembly.  Not all gaps were annotated in the AGP file, this track includes all sequences of N's in the assembly.
</p>
<p>
Genome assembly procedures are covered in the NCBI
<a href="https://www.ncbi.nlm.nih.gov/assembly/basics/"
target=_blank>assembly documentation</a>.<BR>
NCBI also provides
<a href="https://www.ncbi.nlm.nih.gov/assembly/$ncbiAssemblyId"
target="_blank">specific information about this assembly</a>.
</p>
<p>
Any sequence of N's in the assembly is marked as a gap in this track.
The standard gap track only shows the gaps as annotated in the AGP file:
<a href="$hubUrl/$agpFileBase" target=_blank>$asmId.agp.gz</a><br>
The NCBI document
<a href="https://www.ncbi.nlm.nih.gov/assembly/agp/AGP_Specification/"
target=_blank>AGP Specification</a> describes the format of the AGP file.
</p>
<p>
Gaps are represented as black boxes in this track.
There is no information in this track about order or orientation
of the contigs on either side of the gap.
</p>
<p>
_EOF_
    ;
my $gapCount = `bigBedInfo $allGapsBbi | egrep "itemCount:|basesCovered:" | xargs echo | sed -e 's/itemCount/gap count/; s/basesCovered/bases covered/;'`;
chomp $gapCount;
printf "Gap count and coverage: %s</li>\n", $gapCount;

printf "</p>\n";

