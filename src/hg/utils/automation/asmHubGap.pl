#!/usr/bin/env perl

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 4) {
  printf STDERR "usage: asmHubGap.pl asmId asmId.names.tab asmId.agp.gz hubUrl > asmId.gap.html\n";
  printf STDERR "where asmId is the assembly identifier,\n";
  printf STDERR "and   asmId.names.tab is naming file for this assembly,\n";
  printf STDERR "and   asmId.agp.gz is AGP file for this assembly,\n";
  printf STDERR "and   hubUrl is the URL to this assembly directory for the AGP file.\n";
  exit 255;
}

my $asmId = shift;
my $namesFile = shift;
my $agpFile = shift;
my $hubUrl = shift;
my $agpFileBase = basename($agpFile);

if ( ! -s $agpFile ) {
  printf STDERR "ERROR: can not find AGP file:\n\t'%s'\n", $agpFile;
  exit 255;
}

# definition of gap types in the AGP file
my %gapTypes = (
'clone' => 'gaps between clones in scaffolds',
'heterochromatin' => 'heterochromatin gaps',
'short_arm' => 'short arm gaps',
'telomere' => 'telomere gaps',
'centromere' => 'gaps for centromeres are included when they can be reasonably localized',
'scaffold' => 'gaps between scaffolds in chromosome assemblies',
'contig' => 'gaps between contigs in scaffolds',
'repeat' => 'an unresolvable repeat',
'contamination' => 'gap inserted in place of foreign sequence to maintain the coordinates',
'other' => 'gaps added at UCSC to annotate strings of <em>N</em>s that were not marked in the AGP file',
'fragment' => 'gaps between whole genome shotgun contigs'
);

my $em = "<em>";
my $noEm = "</em>";
my $assemblyDate = `grep -v "^#" $namesFile | cut -f9`;
chomp $assemblyDate;
my $ncbiAssemblyId = `grep -v "^#" $namesFile | cut -f10`;
chomp $ncbiAssemblyId;
my $organism = `grep -v "^#" $namesFile | cut -f5`;
chomp $organism;
my $gapCount = `zcat $agpFile | grep -v "^#" | awk -F'\t' '\$5 == "N"' | wc -l`;
chomp $gapCount;
$gapCount = &AsmHub::commify($gapCount);

print <<_EOF_
<h2>Description</h2>
<p>
This track shows the gaps in the $assemblyDate $em${organism}$noEm/$asmId genome assembly.
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
The definition of the gaps in this assembly is from the AGP file:
<a href="$hubUrl/$agpFileBase" target=_blank>$asmId.agp.gz</a><br>
The NCBI document
<a href="https://www.ncbi.nlm.nih.gov/assembly/agp/AGP_Specification/"
target=_blank>AGP Specification</a> describes the format of the AGP file.
</p>
<p>
Gaps are represented as black boxes in this track.
If the relative order and orientation of the contigs on either side
of the gap is supported by read pair data, 
it is a <em>bridged</em> gap and a white line is drawn 
through the black box representing the gap. 
</p>
<p>
This assembly has $gapCount gaps, with the following principal types of gaps:
<ul>
_EOF_
    ;
open (GL, "zcat $agpFile | grep -v '^#' | awk -F'\t' '\$5 == \"N\"' | awk '{print \$7}' | sort | uniq -c | sort -n|") or die "can not read $asmId.agp.gz";
while (my $line = <GL>) {
    chomp $line;
    $line =~ s/^\s+//;
    my ($count, $type) = split('\s+', $line);
    my $minSize = `zcat $agpFile | grep -v "^#" | awk -F'\t' '\$5 == "N"' | awk -F'\t' '\$7 == "'$type'"' | sort -k6,6n | head -1 | awk -F'\t' '{print \$6}'`;
    chomp $minSize;
    my $maxSize = `zcat $agpFile | grep -v "^#" | awk -F'\t' '\$5 == "N"' | awk -F'\t' '\$7 == "'$type'"' | sort -k6,6nr | head -1 | awk -F'\t' '{print \$6}'`;
    chomp $maxSize;
    my $sizeMessage = "";
    if ($minSize == $maxSize) {
        $sizeMessage = sprintf ("all of size %s bases", &AsmHub::commify($minSize));
    } else {
        $sizeMessage = sprintf ("size range: %s - %s bases",
            &AsmHub::commify($minSize), &AsmHub::commify($maxSize));
    }
    if (exists ($gapTypes{$type}) ) {
        printf "<li><B>%s</B> - %s (count: %s; %s)</li>\n", $type,
            $gapTypes{$type}, &AsmHub::commify($count), $sizeMessage;
    } else {
        die "asmHubGap.pl: missing AGP gap type definition: $type";
    }
}
close (GL);

printf "</ul></p>\n";
