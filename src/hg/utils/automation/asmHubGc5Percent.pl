#!/usr/bin/env perl

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;

my $argc = scalar(@ARGV);

if ($argc != 3) {
  printf STDERR "usage: asmHubGc5Percent.pl asmId asmId.names.tab buildDir\n";
  printf STDERR "where asmId is the assembly identifier,\n";
  printf STDERR "and   asmId.names.tab is naming file for this assembly,\n";
  printf STDERR "and   buildDir is the directory with bbi/asmId.gc5Base.bw.\n";
  exit 255;
}

my $asmId = shift;
my $namesFile = shift;
my $buildDir = shift;
my $gc5Bw = "$buildDir/bbi/$asmId.gc5Base.bw";
my $gcOnFly = 0;
if ( ! -s $gc5Bw ) {
  $gc5Bw = "$buildDir/bbi/$asmId.gcOnFly.bw";
  $gcOnFly = 1;
}

if ( ! -s $gc5Bw ) {
  printf STDERR "ERROR: can not find gc5Base.bw or gcOnFly.bw file:\n\t'%s'\n", $gc5Bw;
  exit 255;
}

my @accParts = split('_', $asmId);
my $accession = "$accParts[0]_$accParts[1]";
my $em = "<em>";
my $noEm = "</em>";
my $assemblyDate = `grep -v "^#" $namesFile | cut -f9`;
chomp $assemblyDate;
my $ncbiAssemblyId = `grep -v "^#" $namesFile | cut -f10`;
chomp $ncbiAssemblyId;
my $organism = `grep -v "^#" $namesFile | cut -f5`;
chomp $organism;
my $averageGC = `/cluster/bin/x86_64/bigWigInfo $gc5Bw | egrep "mean:" | sed -e 's/mean: //;'`;
chomp $averageGC;
$averageGC = sprintf("%.2f", $averageGC);

if ( $gcOnFly ) {
  my $asmIdPath = &AsmHub::asmIdToPath($asmId);
  my $twoBitUrl = "https://hgdownload.soe.ucsc.edu/hubs/$asmIdPath/$accession/$accession.2bit";
  printf "<hr><h4>Data Access</h4>\n";
  printf "<p>This track is generated <em>on-the-fly</em> by the browser as needed.\n";
  printf "There is no existing data file for this track.  To obtain the data for this track\n";
  printf "use the following <a href='https://hgdownload.gi.ucsc.edu/downloads.html#utilities_downloads'\n";
  printf " target=_blank>kent command line</a> program <b>hgGcPercent</b>:</p>\n";

  printf "<code>hgGcPercent -wigOut -doGaps -file=stdout -win=5 -verbose=0 test \\<br>&nbsp;&nbsp;&nbsp;%s | gzip -c > %s.varStep.gz</code>\n", $twoBitUrl, $accession;
}

print <<_EOF_
<h2>Description</h2>
<p>
The GC percent track shows the percentage of G (guanine) and C (cytosine) bases
in 5-base windows on
the $assemblyDate $em${organism}$noEm/$asmId/$ncbiAssemblyId genome assembly.
High GC content is typically associated with gene-rich areas.  The average
overall GC percent for the entire assembly is % $averageGC.
</p>

<p>
This track may be configured in a variety of ways to highlight different
aspects of the displayed information. Click the
&quot;Graph configuration help&quot; link for an explanation of the
configuration options.
</p>

<h2>Credits</h2>
<p> The data and presentation of this graph were prepared by
<a href="mailto:&#104;&#105;&#114;a&#109;&#64;&#115;&#111;&#101;
.&#117;&#99;&#115;&#99;.&#101;&#100;u">Hiram Clawson</a>.
</p>
_EOF_
   ;
