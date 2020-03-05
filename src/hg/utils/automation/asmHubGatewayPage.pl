#!/usr/bin/env perl

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;
use File::Basename;

### XXX ### temporary hgdownload-test.gi
### my $sourceServer = "hgdownload-test.gi.ucsc.edu";

my $sourceServer = "hgdownload.soe.ucsc.edu";

my @months = qw( 0 Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec );

sub usage() {
  printf STDERR "usage: asmHubGatewayPage.pl <asmHubName> <pathTo>/*assembly_report.txt <pathTo>/asmId.chrom.sizes <pathTo>/image.jpg <pathTo>/photoCredits.txt\n";
  printf STDERR "output is to stdout, redirect to file: > description.html\n";
  printf STDERR "photoCredits.txt is a two line tag<tab>string file:\n";
  printf STDERR "tags: photoCreditURL and photoCreditName\n";
  printf STDERR "use string 'noPhoto' for image and credits when no photo\n";
  printf STDERR "stderr output is routed to a 'asmId.names.tab' file for use elsewhere\n";
  exit 255;
}

sub chromSizes($) {
  my ($sizeFile) = @_;
  if ( -s $sizeFile ) {
    printf STDERR "# reading chrom.sizes file:\n#\t'%s\'\n", $sizeFile;
    my $ix = 0;
    my $contigCount = 0;

    my %sizes;	# key is contigName, value is size

    if ($sizeFile eq "stdin") {
	while (my $line = <>) {
	    next if ($line =~ m/^\s*#/);
	    ++$contigCount;
	    chomp ($line);
	    my ($name, $size, $rest) = split('\s+', $line, 3);
	    my $key = sprintf("%s_X_%d", $name, $ix++);
	    $sizes{$key} = $size;
	}
    } else {
	open (FH, "<$sizeFile") or die "can not read $sizeFile";
	while (my $line = <FH>) {
	    next if ($line =~ m/^\s*#/);
	    ++$contigCount;
	    chomp ($line);
	    my ($name, $size, $rest) = split('\s+', $line, 3);
	    my $key = sprintf("%s_X_%d", $name, $ix++);
	    $sizes{$key} = $size;
	}
	close (FH);
    }

    my $totalSize = 0;
    foreach my $key (keys %sizes) {
	$totalSize += $sizes{$key}
    }
    my $n50Size = $totalSize / 2;

    my $genomeSize = $totalSize;
    printf "<b>Total assembly nucleotides:</b> %s<br>\n", &AsmHub::commify($totalSize);
    printf "<b>Assembly contig count:</b> %s<br>\n", &AsmHub::commify($contigCount);

    my $prevContig = "";
    my $prevSize = 0;

    $totalSize = 0;
    # work through the sizes until reaching the N50 size
    foreach my $key (sort { $sizes{$b} <=> $sizes{$a} } keys %sizes) {
	$totalSize += $sizes{$key};
	if ($totalSize > $n50Size) {
	    my $prevName = $prevContig;
	    $prevName =~ s/_X_[0-9]+//;
	    my $origName = $key;
	    $origName =~ s/_X_[0-9]+//;
            printf "<b>N50 size:</b> %s<br>\n", &AsmHub::commify($sizes{$key});
	    last;
	}
	$prevContig = $key;
	$prevSize = $sizes{$key};
    }
  } else {
    printf STDERR "# error: can not find chrom.sizes file:\n#\t'%s\'\n",
      $sizeFile;
  }
}

# typical reference:
#   ${inside}/scripts/gatewayPage.pl ${outside}/${asmReport} \
#       > "${inside}/${D}/${B}.description.html" \
#       2> "${inside}/${D}/${B}.names.tab"


my $argc = scalar(@ARGV);

if ($argc != 5) {
  usage;
}

my ($asmHubName, $asmReport, $chromSizes, $jpgImage, $photoCredits) = @ARGV;
if ( ! -s $asmReport ) {
  printf STDERR "ERROR: can not find '$asmReport'\n";
  usage;
}
if ( ! -s $chromSizes ) {
  printf STDERR "ERROR: can not find '$chromSizes'\n";
  usage;
}
if ($jpgImage ne "noPhoto") {
  if ( ! -s $jpgImage ) {
    printf STDERR "ERROR: can not find '$jpgImage'\n";
    usage;
  }
  if ( ! -s $photoCredits ) {
    printf STDERR "ERROR: can not find '$photoCredits'\n";
    usage;
  }
}

my $photoCreditURL = "";
my $photoCreditName = "";
my $imageSize = "";
my $imageName = "";
my $imageWidth = 0;
my $imageHeight = 0;
my $imageWidthBorder = 15;

if ($jpgImage ne "noPhoto") {
  printf STDERR "# reading $photoCredits\n";
  open (FH, "<$photoCredits") or die "can not read $photoCredits";
  while (my $line = <FH>) {
    chomp $line;
    next if ($line =~ m/^#/);
    next if (length($line) < 2);
    my ($tag, $value) = split('\t', $line);
    if ($tag =~ m/photoCreditURL/) {
      $photoCreditURL = $value;
    } elsif ($tag =~ m/photoCreditName/) {
      $photoCreditName = $value;
    }
  }
  close (FH);

  if ( -s $jpgImage ) {
    $imageSize = `identify $jpgImage | awk '{print \$3}'`;
    chomp $imageSize;
    ($imageWidth, $imageHeight) = split('x', $imageSize);
    $imageName = basename($jpgImage);
  }
}

# transform this path name into a chrom.sizes reference

my $thisDir = `pwd`;
chomp $thisDir;
my $ftpName = dirname($thisDir);
my $asmId = basename($ftpName);;
my ($gcXPrefix, $accession, $rest) = split('_', $asmId, 3);
my $accessionId = sprintf("%s_%s", $gcXPrefix, $accession);

my $accessionDir = substr($asmId, 0 ,3);
$accessionDir .= "/" . substr($asmId, 4 ,3);
$accessionDir .= "/" . substr($asmId, 7 ,3);
$accessionDir .= "/" . substr($asmId, 10 ,3);
$accessionDir .= "/" . $accessionId;

my $newStyleUrl = sprintf("%s/%s/%s/%s/%s", $gcXPrefix, substr($accession,0,3),
   substr($accession,3,3), substr($accession,6,3), $asmId);
my $localDataUrl = sprintf("%s/%s/%s/%s/%s", $gcXPrefix, substr($accession,0,3),
   substr($accession,3,3), substr($accession,6,3), $accessionId);
$ftpName =~ s#/hive/data/outside/ncbi/##;
$ftpName =~ s#/hive/data/inside/ncbi/##;
$ftpName =~ s#/hive/data/genomes/asmHubs/##;
# my $urlDirectory = `basename $ftpName`;
# chomp $urlDirectory;
my $speciesSubgroup = $ftpName;
my $asmType = "genbank";
$asmType = "refseq" if ( $speciesSubgroup =~ m#refseq/#);
$speciesSubgroup =~ s#genomes/$asmType/##;;
$speciesSubgroup =~ s#/.*##;;

my %taxIdCommonName;  # key is taxId, value is common name
                      # from NCBI taxonomy database dump
open (FH, "<$ENV{'HOME'}/kent/src/hg/utils/automation/genbank/taxId.comName.tab") or die "can not read taxId.comName.tab";
while (my $line = <FH>) {
  chomp $line;
  my ($taxId, $comName) = split('\t', $line);
  $taxIdCommonName{$taxId} = $comName;
}
close (FH);


my $submitter = "(n/a)";
my $asmName = "(n/a)";
my $orgName = "(n/a)";
my $taxId = "(n/a)";
my $asmDate = "(n/a)";
my $asmAccession = "(n/a)";
my $commonName = "(n/a)";
my $bioSample = "(n/a)";
my $descrAsmType = "(n/a)";
my $asmLevel = "(n/a)";

open (FH, "<$asmReport") or die "can not read $asmReport";
while (my $line = <FH>) {
  chomp $line;
  $line =~ s///g;
  if ($line =~ m/date:\s+/i) {
     next if ($asmDate !~ m#\(n/a#);
     $line =~ s/.*date:\s+//i;
     my ($year, $month, $day) = split('-',$line);
     $asmDate = sprintf("%02d %s %04d", $day, $months[$month], $year);
  }
  if ($line =~ m/biosample:\s+/i) {
     next if ($bioSample !~ m#\(n/a#);
     $line =~ s/.*biosample:\s+//i;
     $bioSample = $line;
  }
  if ($line =~ m/assembly\s+type:\s+/i) {
     next if ($descrAsmType !~ m#\(n/a#);
     $line =~ s/.*assembly\s+type:\s+//i;
     $descrAsmType = $line;
  }
  if ($line =~ m/assembly\s+level:\s+/i) {
     next if ($asmLevel !~ m#\(n/a#);
     $line =~ s/.*assembly\s+level:\s+//i;
     $asmLevel = $line;
  }
  if ($line =~ m/assembly\s+name:\s+/i) {
     next if ($asmName !~ m#\(n/a#);
     $line =~ s/.*assembly\s+name:\s+//i;
     $asmName = $line;
  }
  if ($line =~ m/organism\s+name:\s+/i) {
     next if ($orgName !~ m#\(n/a#);
     $line =~ s/.*organism\s+name:\s+//i;
     $line =~ s/\s+$//;
     $orgName = $line;
  }
  if ($line =~ m/submitter:\s+/i) {
     next if ($submitter !~ m#\(n/a#);
     $line =~ s/.*submitter:\s+//i;
     $submitter = $line;
  }
  if ($line =~ m/$asmType\s+assembly\s+accession:\s+/i) {
     next if ($asmAccession !~ m#\(n/a#);
     $line =~ s/.*$asmType\s+assembly\s+accession:\s+//i;
     $asmAccession = $line;
     $asmAccession =~ s/ .*//;
  }
  if ($line =~ m/taxid:\s+/i) {
     next if ($taxId !~ m#\(n/a#);
     $line =~ s/.*taxid:\s+//i;
     $taxId = $line;
     if (exists($taxIdCommonName{$taxId})) {
       $commonName = $taxIdCommonName{$taxId};
     }
  }
}
close (FH);

$commonName = $orgName if ($commonName =~ m#\(n/a#);
if ($commonName =~ m/\(/) {
   $commonName =~ s/.*\(//;
   $commonName =~ s/\).*//;
}
if ($orgName =~ m/\(/) {
   $orgName =~ s/\(.*//;
}
$orgName =~ s/\s+$//;

printf STDERR "#taxId\tcommonName\tsubmitter\tasmName\torgName\tbioSample\tasmType\tasmLevel\tasmDate\tasmAccession\n";
printf STDERR "%s\t", $taxId;
printf STDERR "%s\t", $commonName;
printf STDERR "%s\t", $submitter;
printf STDERR "%s\t", $asmName;
printf STDERR "%s\t", $orgName;
printf STDERR "%s\t", $bioSample;
printf STDERR "%s\t", $descrAsmType;
printf STDERR "%s\t", $asmLevel;
printf STDERR "%s\t", $asmDate;
printf STDERR "%s\n", $asmAccession;

# printf "<script type='text/javascript'>var asmId='%s';</script>\n", $asmId;

if (length($imageName)) {
printf "<!-- Display image in righthand corner -->
<table align=right border=0 width=%d height=%d>
  <tr><td align=RIGHT><a href=\"https://www.ncbi.nlm.nih.gov/assembly/%s\"
    target=_blank>
    <img src=\"https://%s/hubs/%s/html/%s\" width=%d height=%d alt=\"%s\"></a>
  </td></tr>
  <tr><td align=right>
    <font size=-1> <em>%s</em><BR>
    </font>
    <font size=-2> (Photo courtesy of
      <a href=\"%s\" target=_blank>%s</a>)
    </font>
  </td></tr>
</table>
\n", $imageWidth+$imageWidthBorder, $imageHeight, $asmAccession, $sourceServer, $accessionDir, $imageName, $imageWidth, $imageHeight, $commonName, $orgName, $photoCreditURL, $photoCreditName;
}

my $sciNameUnderscore = $orgName;
$sciNameUnderscore =~ s/ /_/g;
$sciNameUnderscore = "Strigops_habroptilus" if ($orgName =~ m/Strigops habroptila/);

printf "<p>
<b>Common name:</b>&nbsp;%s<br>
<b>Taxonomic name: %s, taxonomy ID:</b> <a href='https://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?id=%s' target='_blank'> %s</a><br>
<b>Sequencing/Assembly provider ID:</b> %s<br>
<b>Assembly date:</b> %s<br>
<b>Assembly type:</b> %s<br>
<b>Assembly level:</b> %s<br>
<b>Biosample:</b> <a href=\"https://www.ncbi.nlm.nih.gov/biosample/?term=%s\" target=\"_blank\">%s</a><br>
<b>Assembly accession ID:</b> <a href=\"https://www.ncbi.nlm.nih.gov/assembly/%s\" target=\"_blank\">%s</a><br>
<b>Assembly FTP location:</b> <a href=\"ftp://ftp.ncbi.nlm.nih.gov/genomes/all/%s\" target=\"_blank\">%s</a><br>
\n", $commonName, $orgName, $taxId, $taxId, $submitter, $asmDate, $descrAsmType,
  $asmLevel, $bioSample, $bioSample, $asmAccession, $asmAccession, $newStyleUrl, $newStyleUrl;

chromSizes($chromSizes);

printf "</p>\n<hr>
<p>
<b>Download files for this assembly hub:</b><br>
To use the data from this assembly for a local hub instance at your
institution, download these data as indicated by these instructions.<br>
<br>
To download this assembly data, use this <em>rsync</em> command:
<pre>
  rsync -a -P \\
    rsync://$sourceServer/hubs/$localDataUrl/ \\
      ./$accessionId/

  which creates the local directory: ./$accessionId/
</pre>
or this <em>wget</em> command:
<pre>
  wget --timestamping -m -nH -x --cut-dirs=6 -e robots=off -np -k \\
    --reject \"index.html*\" -P \"$accessionId\" \\
       https://$sourceServer/hubs/$localDataUrl/

  which creates a local directory: ./$accessionId/
</pre>
<p>
There is an included <em>hub.txt</em> file in that download
data directory to use for your local track hub instance.<br>
Using the genome browser menus: <em><strong>My Data</strong> -&gt; <strong>Track Hubs</strong></em><br>
select the <em><strong>My Hubs</strong></em> tab to enter a URL
to this hub.txt file to attach this assembly hub to a genome browser.
</p>
<p>
The <em>html/$asmId.description.html</em> page is information for your users to
describe this assembly.  This WEB page with these instructions
is an instance of html/$asmId.description.html file.
</p>
<p>
See also: <a href='/goldenPath/help/hgTrackHubHelp.html' target=_blank>track hub help</a> documentation.<br>
</p>\n";

printf "<hr>
<p>
To operate a blat server on this assembly, in the directory where you have
the <em>$asmId.2bit</em> file:
<pre>
gfServer -log=$asmId.gfServer.trans.log -ipLog -canStop start \\
    yourserver.domain.edu 76543 -trans -mask $asmId.2bit &
gfServer -log=$asmId.gfServer.log -ipLog -canStop start \\
    yourserver.domain.edu 76542 -stepSize=5 $asmId.2bit &
</pre>
Adjust the port numbers <em>76543</em> <em>76542</em> and the
<em>yourserver.domain.edu</em> for your local circumstances.<br>
Typically, port numbers in the range <em>49152</em> to <em>65535</em>
are available for private use as in this case.
See also: <a href='https://www.iana.org/assignments/service-names-port-numbers/service-names-port-numbers.xhtml' target=_blank>IANA.org</a> port registry.
</p>
<p>
Enter the following specifications in your <em>genomes.txt</em> file:
<pre>
transBlat yourserver.domain.edu 76543
blat yourserver.domain.edu 76542
</pre>
See also: <a href=\"https://genome.ucsc.edu/goldenPath/help/hubQuickStartAssembly.html#blat\"
target=_blank>Blat for an Assembly Hub</a>
</p>\n";

printf "<hr>
<p>
<b>Search the assembly:</b>
<ul>
<li>
<b>By position or search term: </b> Use the &quot;position or search term&quot;
box to find areas of the genome associated with many different attributes, such
as a specific chromosomal coordinate range; mRNA, EST, or STS marker names; or
keywords from the GenBank description of an mRNA.
<a href=\"http://genome.ucsc.edu/goldenPath/help/query.html\">More information</a>, including sample queries.</li>
<li>
<b>By gene name: </b> Type a gene name into the &quot;search term&quot; box,
choose your gene from the drop-down list, then press &quot;submit&quot; to go
directly to the assembly location associated with that gene.
<a href=\"http://genome.ucsc.edu/goldenPath/help/geneSearchBox.html\">More information</a>.</li>
<li>
<b>By track type: </b> Click the &quot;track search&quot; button
to find Genome Browser tracks that match specific selection criteria.
<a href=\"http://genome.ucsc.edu/goldenPath/help/trackSearch.html\">More information</a>.</li>
</ul>
</p>
<hr>\n";

# printf "<script type='text/javascript' src='../js/gatewayPage.js'></script>\n";

__END__

/hive/data/outside/ncbi/genomes/refseq/vertebrate_mammalian/Homo_sapiens/all_assembly_versions/GCF_000001405.30_GRCh38.p4/GCF_000001405.30_GRCh38.p4_assembly_report.txt

# Assembly Name:  GRCh38.p4
# Description:    Genome Reference Consortium Human Build 38 patch release 4 (GRCh38.p4)
# Organism name:  Homo sapiens (human)
# Taxid:          9606
# Submitter:      Genome Reference Consortium
# Date:           2015-6-25
# Assembly type:  haploid-with-alt-loci
# Release type:   patch
# Assembly level: Chromosome
# Genome representation: full
# GenBank Assembly Accession: GCA_000001405.19 (latest)
# RefSeq Assembly Accession: GCF_000001405.30 (latest)
# RefSeq Assembly and GenBank Assemblies Identical: yes
#
## Assembly-Units:
## GenBank Unit Accession       RefSeq Unit Accession   Assembly-Unit name
## GCA_000001305.2      GCF_000001305.14        Primary Assembly
## GCA_000005045.17     GCF_000005045.16        PATCHES
## GCA_000001315.2      GCF_000001315.2 ALT_REF_LOCI_1

