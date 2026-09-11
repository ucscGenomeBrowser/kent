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
my $hgDownload = "https://hgdownload.soe.ucsc.edu";
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

<hr><h4>Data Access</h4>
_EOF_
   ;

my $asmIdPath = &AsmHub::asmIdToPath($asmId);
my $twoBitUrl = "$hgDownload/hubs/$asmIdPath/$accession/$accession.2bit";

if ( $gcOnFly ) {
  my $bwUrl = "$hgDownload/hubs/$asmIdPath/$accession/bbi/$asmId.gcOnFly.bw";
print <<_EOF_
<p>This track is generated <b>on-the-fly</b> by the browser as needed up
to a data density of 50,000 bases per pixel display.  Greater than that display
density and the display transitions to using the <b>bigWig</b> file:<br>
<br>
  <b><code>$bwUrl</code></b><br>
<br>

You can extract the data from that file with the
<a href='$hgDownload/downloads.html#utilities_downloads'
 target=_blank>kent command line</a> program: <b>bigWigToWig</b>:<br>

<br>
<b><code>bigWigToWig $bwUrl stdout \\<br>&nbsp;&nbsp;&nbsp;| gzip -c &gt; $accession.gcOnFly.varStep.gz</code></b><br>
<br>

That <b>bigWig</b> data was calculated with the <b>hgGcPercent</b> command
with the window size of <b>-win=50000</b>.<br>
<br>
To obtain the traditional 5-base window  data for this track
use the following <a href='$hgDownload/downloads.html#utilities_downloads'
 target=_blank>kent command line</a> program <b>hgGcPercent</b>:<br>

<br>
<b><code>hgGcPercent -wigOut -doGaps -file=stdout -win=5 -verbose=0 test \\<br>&nbsp;&nbsp;&nbsp;$twoBitUrl \\<br>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;| gzip -c &gt; ${accession}.varStep.gz</code></b>
</p>
_EOF_
  ;
} else {
  my $bwUrl = "$hgDownload/hubs/$asmIdPath/$accession/bbi/$asmId.gc5Base.bw";
print <<_EOF_
<p>This track is displayed from the <b>bigWig</b> file:<br>

<br>
   <b><code>$bwUrl</code></b><br>
<br>

You can extract the data from that file with the
<a href='$hgDownload/downloads.html#utilities_downloads'
 target=_blank>kent command line</a> program: <b>bigWigToWig</b>:<br>

<br>
<b><code>bigWigToWig $bwUrl stdout \\<br>&nbsp;&nbsp;&nbsp;| gzip -c &gt; $accession.gc5Base.varStep.gz</code></b><br>
<br>

Or, you can calculate that data locally at the 5-base window size
with the following <a href='$hgDownload/downloads.html#utilities_downloads'
 target=_blank>kent command line</a> program <b>hgGcPercent</b>:<br>
<br>

<b><code>hgGcPercent -wigOut -doGaps -file=stdout -win=5 -verbose=0 test \\<br>&nbsp;&nbsp;&nbsp;$twoBitUrl \\<br>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;| gzip -c &gt; ${accession}.varStep.gz</code></b>
</p>
_EOF_
  ;
}
print <<_EOF_
<h2>Credits</h2>
<p> The data and presentation of this graph were prepared by
<a href="mailto:&#104;&#105;&#114;a&#109;&#64;&#115;&#111;&#101;
.&#117;&#99;&#115;&#99;.&#101;&#100;u">Hiram Clawson</a>.
</p>
_EOF_
   ;
