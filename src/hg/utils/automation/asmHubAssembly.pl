#!/usr/bin/env perl

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 4) {
  printf STDERR "usage: asmHubAssembly.pl asmId asmId.names.tab asmId.agp.gz hubUrl > asmId.gap.html\n";
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

# definition of contig types in the AGP file
my %goldTypes = (
'A' => 'active finishing',
'D' => 'draft sequence',
'F' => 'finished sequence',
'O' => 'other sequence',
'P' => 'pre draft',
'W' => 'whole genome shotgun'
);

my $em = "<em>";
my $noEm = "</em>";
my $assemblyDate = `grep -v "^#" $namesFile | cut -f9`;
chomp $assemblyDate;
my $ncbiAssemblyId = `grep -v "^#" $namesFile | cut -f10`;
chomp $ncbiAssemblyId;
my $organism = `grep -v "^#" $namesFile | cut -f5`;
chomp $organism;
my $partCount = `zcat $agpFile | grep -v "^#" | awk -F'\t' '\$5 != "N" && \$5 != \"U\"' | wc -l`;
chomp $partCount;
$partCount = &AsmHub::commify($partCount);

print <<_EOF_
<h2>Description</h2>
<p>
This track shows the sequences used in the $assemblyDate $em${organism}$noEm/$asmId genome assembly.
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
In dense mode, this track depicts the contigs that make up the
currently viewed scaffold.
Contig boundaries are distinguished by the use of alternating gold and brown
coloration. Where gaps
exist between contigs, spaces are shown between the gold and brown
blocks.  The relative order and orientation of the contigs
within a scaffold is always known; therefore, a line is drawn in the graphical
display to bridge the blocks.</p>
<p>
This assembly has $partCount component parts, with the following principal types of parts:
<ul>
_EOF_
    ;
open (GL, "zcat $agpFile | grep -v '^#' | awk -F'\t' '\$5 != \"N\" && \$5 != \"U\"' | awk '{print \$5}' | sort | uniq -c | sort -n|") or die "can not read $asmId.agp.gz";
while (my $line = <GL>) {
   chomp $line;
   $line =~ s/^\s+//;
   my ($count, $type) = split('\s+', $line);
   my $singleMessage = "";
   if ((1 == $count) && (("F" eq $type) || ("O" eq $type))) {
       my $chr = `zcat $agpFile | grep -v '^#' | awk -F'\t' '\$5 == \"$type\" | cut -f1'`;
       chomp $chr;
       my $frag = `zcat $agpFile | grep -v '^#' | awk -F'\t' '\$5 == \"$type\" | cut -f6'`;
       chomp $frag;
       $singleMessage = sprintf("(%s/%s)", $chr, $frag);
   }
   if (exists ($goldTypes{$type}) ) {
      if (length($singleMessage)) {
         printf "<li>%s - one %s %s</li>\n", $type, $goldTypes{$type}, $singleMessage;
      } else {
         printf "<li>%s - %s (count: %s)</li>\n", $type, $goldTypes{$type}, &AsmHub::commify($count);
      }
   } else {
      printf STDERR "'%s'\n", $line;
      die "asmHubAssembly.pl: missing AGP contig type definition: $type";
   }
}
close (GL);
printf "</ul></p>\n";
