#!/usr/bin/env perl

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;

my $argc = scalar(@ARGV);

if ($argc != 3) {
  printf STDERR "usage: asmHubWindowMasker.pl asmId asmId.names.tab buildDir\n";
  printf STDERR "where asmId is the assembly identifier,\n";
  printf STDERR "and   asmId.names.tab is naming file for this assembly,\n";
  printf STDERR "and   buildDir is the directory with bbi/asmId.windowMasker.bb\n";
  exit 255;
}

my $asmId = shift;
my $namesFile = shift;
my $buildDir = shift;
my $wmBbi = "$buildDir/bbi/$asmId.windowMasker.bb";
my $chromSizes = "$buildDir/$asmId.chrom.sizes";

if ( ! -s $wmBbi ) {
  printf STDERR "ERROR: can not find windowMasker bbi file:\n\t'%s'\n", $wmBbi;
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
my $basesCovered = `bigBedInfo $wmBbi | egrep "basesCovered:" | sed -e 's/basesCovered: //;'`;
chomp $basesCovered;
my $bases = $basesCovered;
$bases =~ s/,//g;
my $asmSize = &AsmHub::asmSize($chromSizes);
my $percentCoverage = sprintf("%.2f", (100.0 * $bases) / $asmSize);
$asmSize = &AsmHub::commify($asmSize);

print <<_EOF_
<h2>Description</h2>
<p>
This track depicts masked sequence as determined by <a href="
http://bioinformatics.oxfordjournals.org/content/22/2/134.full" target="_blank">WindowMasker</a> on the
the $assemblyDate $em${organism}$noEm/$asmId/$ncbiAssemblyId genome assembly.
The WindowMasker tool is included in the NCBI C++ toolkit. The source code
for the entire toolkit is available from the NCBI
<a href="ftp://ftp.ncbi.nih.gov/toolbox/ncbi_tools++/CURRENT/" target="_blank">
FTP site</a>.
</p>

<h2>Methods</h2>
<p>
To create this track, WindowMasker was run with the following parameters:
<pre>
windowmasker -mk_counts true -input $asmId.unmasked.fa -output wm_counts
windowmasker -ustat wm_counts -sdust true -input $asmId.unmasked.fa -output windowmasker.intervals
perl -wpe 'if (s/^>lcl\\|(.*)\\n\$//) { \$chr = \$1; } \\
   if (/^(\\d+) - (\\d+)/) { \\
   \$s=\$1; \$e=\$2+1; s/(\\d+) - (\\d+)/\$chr\\t\$s\\t\$e/; \
   }' windowmasker.intervals > windowmasker.sdust.bed
</pre>
The windowmasker.sdust.bed included masking for areas of the
assembly that are gap.  The file was 'cleaned' to remove those areas
of masking in gaps, leaving only the sequence masking.  The final
result covers $basesCovered bases in the assembly size $asmSize for
a percent coverage of % $percentCoverage.
</p>

<h2>References</h2>

<p>
Morgulis A, Gertz EM, Sch&auml;ffer AA, Agarwala R.
<a href="http://bioinformatics.oxfordjournals.org/content/22/2/134.full" target="_blank">
WindowMasker: window-based masker for sequenced genomes</a>.
<em>Bioinformatics</em>. 2006 Jan 15;22(2):134-41.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/16287941" target="_blank">16287941</a>
</p>
_EOF_
   ;
