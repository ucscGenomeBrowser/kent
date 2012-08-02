#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit ~/kent/src/hg/utils/mkPhotoGateway.pl instead.

# $Id: mkPhotoGateway.pl,v 1.2 2007/06/01 19:39:14 galt Exp $

use warnings;
use strict;

sub usage() {
    print "usage: ./mkPhotoGateway.pl <file.html>\n";
    print "\tthe resulting page is written to the file.html specified\n";
    print "\tCurrent contents of dbDb.hgcentraltest are used to construct\n";
    print "\tthe resulting gateway page.\n";
}

my $argc=scalar(@ARGV);

if ($argc != 1) {
    usage; die "ERROR: Please supply a single file name for results.\n";
}

my $resultFile = shift;
open (FH,">$resultFile") or die "ERROR: can not write to $resultFile";

my @dbs = split('\n',
    `hgsql -N -e "select name from dbDb order by orderKey;" hgcentraltest`);
my @sciNames = split('\n',
    `hgsql -N -e "select scientificName from dbDb order by orderKey;" hgcentraltest`);
die "incorrect db vs. sciName count" if (scalar(@dbs) != scalar(@sciNames));
printf STDERR "in dbDb: dbs: %d == %d scientific names\n",
	scalar(@dbs), scalar(@sciNames);

my @dbImages;

for (my $i = 0; $i < scalar(@dbs); ++$i) {
    chomp ($dbs[$i]);
    chomp ($sciNames[$i]);
    next if ($dbs[$i] =~ m/hg100/);
    next if ($dbs[$i] =~ m/canHg/);
    $sciNames[$i] =~ s/ /_/g;
    my $jpgName = "$sciNames[$i].jpg";
    if ( -s "/usr/local/apache/htdocs/images/$jpgName" ) {
	push @dbImages,$dbs[$i];
    }
}

my @useDbs;
my $prevDb = "";
my $prevVer = -1;
my $latestDb = "xxx";
for (my $i = 0; $i < scalar(@dbImages); ++$i) {
    next if ($dbImages[$i] =~ m/^rheMac[0-9][a-z]$/);
    next if ($dbImages[$i] =~ m/^pt0$/);
    if (length($prevDb) > 0) {
	$dbImages[$i] =~ m/[0-9]+$/;
	my $dbVersion = $&;
	my $stripDigits = $`;
	if ($stripDigits =~ m/^$prevDb$/) {
	    if ($dbVersion > $prevVer) {
		$prevVer = $dbVersion;
		$latestDb = $dbImages[$i];
	    }
	} else {
	    push @useDbs,$latestDb;
	    $prevVer = $dbVersion;
	    $latestDb = $dbImages[$i];
	}
	$prevDb = $stripDigits;
    } else {
	$dbImages[$i] =~ m/[0-9]+$/;
	$prevVer = $&;
	$latestDb = $dbImages[$i];
	$prevDb = $dbImages[$i];
	$prevDb =~ s/[0-9]+$//;
    }
}
push @useDbs,$latestDb;

printf STDERR "found %d images, %d latest dbs\n",
	scalar(@dbImages), scalar(@useDbs);

print FH "<HTML><HEAD><TITLE>Genome Browser Photo Gateway</TITLE>\n";
print FH '<LINK REL="STYLESHEET" HREF="/style/HGStyle.css">', "\n";
print FH "</HEAD>\n<BODY BACKGROUND=\"../images/floretHiram.jpg\"";
print FH "\t",
	'BGCOLOR="#FFF9D2" LINK="#0000CC" VLINK="#330066" ALINK="#6600FF"',
	">\n";
print FH "<TABLE ALIGN=LEFT BORDER=0>\n";
print FH '<TR><TH COLSPAN=3 ALIGN=LEFT><IMG SRC="/images/title.jpg" ALT="UCSC Genome Bioinformatics"></TH></TR>', "\n";
print FH '<TR><TD COLSPAN=2 HEIGHT=40><!-- This file generates the top menu bar on Genome Browser home page-->
<!-- which has a different menu from the other static pages------------>

  <TABLE BGCOLOR="000000" CELLPADDING="1" CELLSPACING="1" WIDTH="100%" HEIGHT="27">
    <TR BGCOLOR="2636D1"><TD VALIGN="middle">
      <TABLE BORDER=0 CELLSPACING=0 CELLPADDING=0 BGCOLOR="2636D1" HEIGHT="24">
	<TR><TD VALIGN="middle"><FONT COLOR="#89A1DE">&nbsp;&nbsp;

	  <A HREF="/index.html" class="topbar">HOME</A>&nbsp;&nbsp;-&nbsp;&nbsp;
	  <A HREF="/cgi-bin/hgGateway" class="topbar">Genomes</A>&nbsp;&nbsp;-&nbsp;&nbsp;
 	  <A HREF="/cgi-bin/hgBlat?command=start" class="topbar">Blat</A>&nbsp;&nbsp;-&nbsp;&nbsp; 
	  <A HREF="/cgi-bin/hgTables?command=start" class="topbar">Tables</A>&nbsp;&nbsp;-&nbsp;&nbsp; 
	  <A HREF="/cgi-bin/hgNear" class="topbar">Gene Sorter</A>&nbsp;&nbsp;-&nbsp;&nbsp; 
	  <A HREF="/cgi-bin/hgPcr?command=start" class="topbar">PCR</A>&nbsp;&nbsp;-&nbsp;&nbsp; 
	  <A HREF="/cgi-bin/hgVisiGene?command=start" class="topbar">VisiGene</A>&nbsp;&nbsp;-&nbsp;&nbsp; 
	  <A HREF="/FAQ/" class="topbar">FAQ</A>&nbsp;&nbsp;-&nbsp;&nbsp; 
	  <A HREF="/goldenPath/help/hgTracksHelp.html" class="topbar">Help</A>

	</FONT></TD></TR>
      </TABLE>
    </TD></TR>
  </TABLE>
<!-- end topbar -->
</TD></TR>', "\n";

my $done = 0;
while ($done < scalar(@useDbs)) {
    print FH "<TR>\n";
    for (my $i = 0; $i < 3; ++$i) {
	if ($done < scalar(@useDbs)) {
	    my $href=`hgsql -N -e 'select genome,name from dbDb where name="$useDbs[$done]";' hgcentraltest`;
	    chomp $href;
	    my $genomeOrg = $href;
	    $genomeOrg =~ s/\t/ - /;
	    $href =~ s/\t/&db=/;
	    $href =~ s/ /+/g;
	    my $sciName=`hgsql -N -e 'select scientificName from dbDb where name="$useDbs[$done]";' hgcentraltest`;
	    chomp $sciName;
	    $sciName =~ s/ /_/g;
	    printf    " <TD>%s</TD>", $genomeOrg;
	    printf FH "  <TD ALIGN=CENTER><A HREF=\"/cgi-bin/hgTracks?org=%s\" TARGET=_blank>\n", $href;
	    printf FH "   <IMG SRC=\"/images/%s.jpg\"></A><BR>", $sciName;
	    printf FH "    %s</A><BR>\n", $genomeOrg;
	    printf FH "    (<A HREF=\"/cgi-bin/hgGateway?org=%s\">photo credits</A>)</TD>\n", $href;
	    ++$done;
	} else {
	    printf "  <TD>&nbsp;</TD>";
	    print FH "  <TD>&nbsp;</TD>\n";
	}
    }
    printf "\n";
    print FH "</TR>\n";
}
print FH "</BODY></HTML>\n";

close (FH);
