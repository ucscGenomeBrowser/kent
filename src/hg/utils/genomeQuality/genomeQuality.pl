#! /usr/local/bin/perl

use strict;
use warnings;

# use CGI::Carp qw(fatalsToBrowser carpout);

use CGI qw(:standard);	#	using the CGM.pm module
			# reference documentaion for CGI.pm is at:
			# http://stein.cshl.org/WWW/software/CGI/
			# moved to:
			# http://search.cpan.org/dist/CGI/lib/CGI.pm

my @columnHeaders = ("dataBase", "n50Size", "totalSize", "sizePercent",
	"n50Count", "contig1000", "cValue", "percentCover",
	"coverage", "commonName", "sciName", "Ns",
	"rmsk", "TRF", "wmask", "score");

# removed: "contigCount" "contigPercent"
# removed: "-k6,6n" "-k7.7n"

my @columnSorts = ("-k1,1", "-k2,2n", "-k3,3n", "-k4,4n",
	"-k5,5n", "-k16,16n", "-k8,8n", "-k9,9n",
	"-k17,17n", "-k10,10", "-k11,11", "-k12,12n",
	"-k13,13n", "-k14,14n", "-k15,15n", "-k18,18n");

my $colCount = scalar(@columnHeaders);
my $checkCount = scalar(@columnSorts);

die "columnHeaders($colCount) and columnSorts($checkCount) arrays are out of synchronization"
    if ($colCount != $checkCount);

my $debugOn = 0;
my $sortAction = "dataBase";
my $toggleRev = "";
my $prevSort = "n50Size";
my $dbSubsets = "RR active";
my $refreshButton = "";
my $prevToggle = "";

sub columnHeaders() {
print "<TR>\n";
for (my $i = 0; $i < scalar(@columnHeaders); ++$i) {
    if ($sortAction eq $columnHeaders[$i]) {
	print '    <TH CLASS="bgSelected"><INPUT TYPE="submit" NAME="sortColumn" VALUE="',
	    $columnHeaders[$i], '" /></TH>', "\n";
    } else {
	print '    <TH><INPUT TYPE="submit" NAME="sortColumn" VALUE="',
	    $columnHeaders[$i], '" /></TH>', "\n";
    }
}
printf "</TR>\n";
}

sub scoreLegend() {
print "<TABLE>\n";
print "<TR><TH>scoring:</TH>\n";
print "    <TH CLASS='highConf'>good - 10 points</TH>\n";
print "    <TH CLASS='mediumConf'>medium - 5 points</TH>\n";
print "    <TH CLASS='lowConf'>low - 1 point</TH>\n";
print "    <TH CLASS='noConf'>N/A - 0 points</TH>\n";
print "</TR>\n";
print "</TABLE>\n";
}

sub scoreFunction($$$$) {
    my ($valueString, $low, $high, $highLow) = @_;
    my ($value, $other) = split('\s+', $valueString, 2);
    if ($value eq "N/A") {
	printf "    <TD CLASS='noConf'>%s</TD>\n", $value;
    } else {
	if ($highLow eq "highGood") {
	    if ($value > $high) {
		printf "    <TD CLASS='highConf'>%s</TD>\n", $valueString;
	    } elsif ($value > $low) {
		printf "    <TD CLASS='mediumConf'>%s</TD>\n", $valueString;
	    } else {
		printf "    <TD CLASS='lowConf'>%s</TD>\n", $valueString;
	    }
	} else {
	    if ($value < $low) {
		printf "    <TD CLASS='highConf'>%s</TD>\n", $valueString;
	    } elsif ($value < $high) {
		printf "    <TD CLASS='mediumConf'>%s</TD>\n", $valueString;
	    } else {
		printf "    <TD CLASS='lowConf'>%s</TD>\n", $valueString;
	    }
	}
    }
}

foreach my $P(param()) {
    if ($P eq "toggle") {
	$toggleRev = param($P);
    } elsif ($P eq "debug") {
	$debugOn = 1;
        $debugOn = 0 if ("0" eq param($P));
    } elsif ($P eq "refresh") {
	$refreshButton = param($P);
    } elsif ($P eq "sortColumn") {
	$sortAction = param($P);
    } elsif ($P eq "prevToggle") {
	$prevToggle = param($P);
    } elsif ($P eq "dbSubsets") {
	$dbSubsets = param($P);
    } elsif ($P eq "prevSort") {
	$prevSort = param($P);
    }
}

if (length($toggleRev)) {
    $prevToggle = $toggleRev;
}

if ($sortAction eq $prevSort) {
    if ($toggleRev eq "r") {
	$toggleRev = ""
    } else {
	$toggleRev = "r"
    }
} else {
    $toggleRev = ""
}
# do not change sort or toggleRev if refresh button
if ($refreshButton eq "refresh") {
    $sortAction = $prevSort;
    $toggleRev = $prevToggle;
}

my %rrActiveList = ();	# key is database name, value is 1

open (FH, "<rr.active.list") or die "can not read rr.active.list";
while (my $dbName = <FH>) {
    next if $dbName =~ m/^#/;
    chomp $dbName;
    $rrActiveList{$dbName} = 1;
}
close (FH);

my $dataFile="dataStats.txt";

my %newestDb = ();	# key is db name without version, value is highest ver
open (FH, "cut -f1 $dataFile|") or die "can not cut -f1 $dataFile";
while (my $db = <FH>) {
    next if ($db =~ m/^#/);
    chomp $db;
    my $dbStrip = $db;
    $dbStrip =~ s/([0-9]+$)//;
    my $ver = $1;
    if (exists($newestDb{$dbStrip})) {
        $newestDb{$dbStrip} = $ver if $ver > $newestDb{$dbStrip};
    } else {
        $newestDb{$dbStrip} = $ver;
    }

}
close (FH);

print header(-expires=>'now');
print "\n";
print start_html(-title=>"Genome assembly quality estimate",
	-style=>"http://genome-test.soe.ucsc.edu/~hiram/genomeStats/genomeStats.css");
print "\n<HR>";

if ($debugOn) {
print "<TABLE>\n";
print "<TR><TH COLSPAN=2>prevSort: $prevSort, sortAction: $sortAction toggle: $toggleRev<BR>refreshButton: $refreshButton</TH></TR>\n";
print "<TR><TH COLSPAN=2>param() function returns values</TH></TR>\n";
print "<TR><TH> Field Name </TH><TH> Field Contents </TH></TR>\n";
foreach my $p(param()) {
	print "<TR> <TD> $p </TD><TD>", param($p), "</TD></TR>\n";
}
print "</TABLE><HR>\n";
}

my $sortCmd = sprintf ("sort -t'\t' ");
for (my $i = 0; $i < scalar(@columnSorts); ++$i) {
    if ($sortAction eq $columnHeaders[$i]) {
	$sortCmd .= $columnSorts[$i];
	last;
    }
}
$sortCmd .= $toggleRev;

if (length($sortCmd) > 0) {
    open (FH, "$sortCmd $dataFile|") or die "can not '$sortCmd $dataFile'";
} else {
    open (FH, "<$dataFile") or die "can not read $dataFile";
}

print start_form(-action=>'sort.pl'), "\n";

# keep debugOn if it was turned on, else forget it
if ($debugOn) {
    param(-name=>'debug', -value=>"1");
    print hidden(-name=>'debug', -default=>["1"]), "\n";
}

param(-name=>'prevSort', -value=>"$sortAction");
param(-name=>'toggle', -value=>"$toggleRev");
print hidden(-name=>'prevSort', -default=>["$sortAction"]), "\n";
print hidden(-name=>'toggle', -default=>["$toggleRev"]), "\n";

print "<H2>Genome assembly quality estimate</H2>\n";

scoreLegend;

print "<TABLE>\n";
print "<TR><TH COLSPAN=$colCount><A HREF='#legend'>view legend at bottom of page</A></TH></TR>\n";
print "<TR><TH COLSPAN=$colCount>select subset to display:&nbsp;";
print radio_group(-name=>'dbSubsets',
        -values=>['RR active', 'newest', 'all'],
        -default=>'RR active');
print '&nbsp;&nbsp;databases.&nbsp;&nbsp;<INPUT TYPE="submit" NAME="refresh" VALUE="refresh" /></TH></TR>', "\n";

print "<TR><TH COLSPAN=$colCount>sorted on column: '$sortAction', click column headers to toggle sort and reverse sort</TH></TR>\n";

columnHeaders;

while (my $line = <FH>) {
    chomp $line;
    my ($db, $n50Size, $totalSize, $percentSize, $n50Count,
	$contigCount, $percentCount, $cValue, $percentCover, $commonName,
	$sciName, $Ns, $rmsk, $TRF, $wmask, $contig1000, $coverage, $score) =
		split('\t', $line);
    if ($dbSubsets eq "RR active") {
	next if (!exists($rrActiveList{$db}));
    } elsif ($dbSubsets eq "newest") {
	my $dbStrip = $db;
	$dbStrip =~ s/([0-9]+$)//;
	my $ver = $1;
	next if (!exists($newestDb{$dbStrip}) || $ver != $newestDb{$dbStrip});
    }
    my $highScore = 10;
    my $mediumScore = 5;
    my $lowScore = 1;
    printf "<TR><TD>%s</TD>\n", $db;
    scoreFunction($n50Size, 100000, 1000000, "highGood");
    printf "    <TD CLASS='right'>%d</TD>\n", $totalSize;
    scoreFunction($percentSize, 0.1, 1.0, "highGood");
    scoreFunction($n50Count, 100, 1000, "lowGood");
    scoreFunction($contig1000, 1000, 10000, "lowGood");
#	scoreFunction($contigCount, 1000, 10000, "lowGood");
#	scoreFunction($percentCount, 1, 10, "highGood");
    printf "    <TD CLASS='right'>%s</TD>\n", $cValue;
    scoreFunction($percentCover, 75, 90, "highGood");
    scoreFunction($coverage . " X", 3, 6, "highGood");
    printf "    <TD CLASS='right'>%s</TD>\n", $commonName;
    printf "    <TD CLASS='right'>%s</TD>\n", $sciName;
    scoreFunction($Ns, 10, 25, "lowGood");
    scoreFunction($rmsk, 10, 25, "highGood");
    scoreFunction($TRF, 1, 2, "highGood");
    scoreFunction($wmask, 25, 40, "highGood");
    scoreFunction($score, 50, 70, "highGood");
    printf "</TR>\n";
}
close (FH);
columnHeaders;
scoreLegend;
printf "</TABLE><HR>\n";

print end_form, "\n";

print <<'LEGEND_TEXT';
<A NAME='legend'>
Measurements come from:</A>
<UL>
<LI>n50Size - calculated from contigs that have only bridged gaps</LI>
<LI>totalSize - sum total size of the contigs with bridged gaps only,
	not including chrM, chrUn, haplotypes _hap, or _random</LI>
<LI>sizePercent - (100 * n50Size) / totalSize</LI>
<LI>n50Count - the sequential number of the contig at the N50 size</LI>
<LI>contig1000 - contig count for contigs over 1000 in length</LI>
<LI>cValue - pico gram estimate of total genome size from <A HREF="http://genomesize.com" TARGET=_blank>genomesize.com</A> these are very rough values with a lot of slack</LI>
<LI>percentCover - (100 * totalSize) / estimatedSize where estimatedSize is cValue * 0.978 x 10^9</LI>
<LI>coverage - claimed coverage from assembly center</LI>
<LI>commonName - from UCSC dbDb table in hgcentral database</LI>
<LI>sciName - scientific name from UCSC dbDb table in hgcentral database</LI>
<LI>Ns - percent of N (unknown) bases in complete assembly</LI>
<LI>rmsk - featureBits on rmsk table for complete assembly, percent of assembly masked by repeat masker</LI>
<LI>TRF - featureBits on simpleRepeat table for complete assembly, percent of assembly masked by Tandem Repeat Finder</LI>
<LI>wmask - featureBits on windowmaskerSdust table for complete assembly, percent of assembly masked by WindowMasker</LI>
<LI>score - 10 points for good quality measurement, 5 points for medium, 1 for low quality, 0 for N/A</LI>
</UL>

Future additions: add a measurement of gene coverage of some sort.<BR>
The N count and repeat masking is currently over the whole genome,<BR>
may be better to count these numbers only in the contigs being measured.<BR>
Gaps - number of gaps, or total gap content may be interesting.<BR>
Add sequencing/assembly center indication.

</P>
<HR>

LEGEND_TEXT

# <LI>contigCount - the number of contigs with only bridged gaps</LI>
# <LI>contigPercent - (100 * n50Count) / contigCount</LI>

print end_html, "\n";		#	Complete the HTML
