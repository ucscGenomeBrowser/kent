#!/usr/bin/perl

# mkOrthologFrame - generate ENCODE region frame page including orthologs, from regions file
#       and description file
# positions file: <chrN:x-y>\t<region>
# description file: <region>|<description>
# header file: HTML header for file

#use strict;

@ARGV == 7 or die "usage: mkOrthologFrame <description-file> <position-file> <header-file> <assembly> <consensus-bed-file> <liftOver-bed-file> <Mercator-bed-file>\n";
my ($descriptionFile, $positionFile, $headerFile, $genome, $consensusFile, $liftOverFile, $MercatorFile) = @ARGV;

open(POSITIONS, $positionFile) or die "ERROR: can't open $regionFile\n";
open(DESCRS, $descriptionFile) or die "ERROR: can't open $descriptionFile\n";
open(HEADER, $headerFile) or die "ERROR: can't open $headerFile\n";
open(CONSENSUS, $consensusFile) or die "ERROR: can't open $consensusFile\n";
open(LIFTOVER, $liftOverFile) or die "ERROR: can't open $liftOverFile\n";
open(MERCATOR, $MercatorFile) or die "ERROR: can't open $MercatorFile\n";

# print header
system("cat $headerFile");

# read in positions file

my %positions = ();
while (<POSITIONS>) {
    chomp;
    my ($position, $region) = split /\s/;
    $positions{$region} = $position;
}

# read in description file
my %descriptions = ();
while (<DESCRS>) {
    chomp;
    my ($region, $description) = split /\|/;
    $descriptions{$region} = $description;
}

# read in consensus ortholog regions file
my %orthoRegionParts = ();
my %orthoRegionPartPosition = ();
while (<CONSENSUS>) {
    chomp;
    my ($chr, $start, $end, $regionPart) = split /\s/;
    $orthoRegionPartPosition{$regionPart} = 
            sprintf "%s:%d-%d", $chr, $start, $end;
    my ($region) = split (/_/, $regionPart);
    if (defined($orthoRegionParts{$region})) {
        $orthoRegionParts{$region} = 
            join (",", $orthoRegionParts{$region}, $regionPart);
    } else {
        $orthoRegionParts{$region} = $regionPart;
    }
}

# read in liftOver regions file
my %LORegionParts = ();
my %LORegionPartPosition = ();
while (<LIFTOVER>) {
    chomp;
    my ($chr, $start, $end, $regionPart) = split /\s/;
    $LORegionPartPosition{$regionPart} = 
            sprintf "%s:%d-%d", $chr, $start, $end;
    my ($region) = split (/_/, $regionPart);
    if (defined($LORegionParts{$region})) {
        $LORegionParts{$region} = 
            join (",", $LORegionParts{$region}, $regionPart);
    } else {
        $LORegionParts{$region} = $regionPart;
    }
}

# read in Mercator regions file
my %MCRegionParts = ();
my %MCRegionPartPosition = ();
while (<MERCATOR>) {
    chomp;
    my ($chr, $start, $end, $regionPart) = split /\s/;
    $MCRegionPartPosition{$regionPart} = 
            sprintf "%s:%d-%d", $chr, $start, $end;
    my ($region) = split (/_/, $regionPart);
    if (defined($MCRegionParts{$region})) {
        $MCRegionParts{$region} = 
            join (",", $MCRegionParts{$region}, $regionPart);
    } else {
        $MCRegionParts{$region} = $regionPart;
    }
}

# print table header
print "<table border cellpadding=\"2\">\n";
print "<TR><TH>Region<TH>Description<TH>Chr<TH>Size (Mb)<TH>Consensus $genome<TH>Size<TH>Ratio<TH>LiftOver<TH>Size<TH>Mercator<TH>Size<TH>Diff (Kb)</TR>\n";

# print table entries
foreach $region (sort keys %descriptions) {
    $_ = $positions{$region};
    my ($chr, $start, $end) = /^chr(\S+):(\d+)-(\d+)$/;
    my $length = $end - $start + 1;
    my $regionLength = $length;
    $length = $length/1000;
    $length = $length/1000;
    $genomeUpper = "\u$genome";

    # human browser - region link, description, and size
    printf "<TR><TD><A HREF=\"/cgi-bin/hgTracks?db=hg16&amp;position=%s&amp;encodeRegions=full&amp;net%s=full&amp;refGene=pack\" TARGET=browser>%s</A><TD>%s<TD ALIGN=\"right\">%s<TD ALIGN=\"center\">%.1f", 
        $positions{$region}, $genomeUpper, $region, $descriptions{$region}, 
        $chr, $length;

    # consensus regions in ortholog browser -  region links, size, 
    #   and size ratio
    @parts = split (/,/, $orthoRegionParts{$region});
    $length = 0;
    printf "<TD>";
    foreach $part (@parts) {
        my ($chr, $range) = split (/:/, $orthoRegionPartPosition{$part});
        my ($start, $end) = split (/-/, $range);
        $length = $length + ($end - $start + 1);
        printf "<A HREF=\"/cgi-bin/hgTracks?db=%s&amp;encodeRegionConsensus=full&amp;position=%s&amp;netHg16=full&amp;refGene=pack\" TARGET=browser2>%s</A> &nbsp;", $genome, $orthoRegionPartPosition{$part}, $chr;
    }
    my $ratio = $length * 100 / $regionLength;
    $length = $length/1000;
    $length = $length/1000;
    printf "<TD>%.1f</TR>\n", $length;
    printf "<TD ALIGN=right>%3.0f%%</TR>\n", $ratio;

    # liftOver regions in ortholog browser - region links, size
    @parts = split (/,/, $LORegionParts{$region});
    $length = 0;
    printf "<TD>";
    foreach $part (@parts) {
        my ($chr, $range) = split (/:/, $LORegionPartPosition{$part});
        my ($start, $end) = split (/-/, $range);
        $length = $length + ($end - $start + 1);
        printf "<A HREF=\"/cgi-bin/hgTracks?db=%s&amp;encodeRegions2=full&amp;position=%s&amp;netHg16=full&amp;refGene=pack\" TARGET=browser2>%s</A> &nbsp;", $genome, $LORegionPartPosition{$part}, $chr;
    }
    my $LOlength = $length;
    $length = $length/1000;
    $length = $length/1000;
    printf "<TD>%.1f</TR>\n", $length;

    # Mercator regions in ortholog browser - region links, size, diff
    @parts = split (/,/, $MCRegionParts{$region});
    $length = 0;
    printf "<TD>";
    foreach $part (@parts) {
        my ($chr, $range) = split (/:/, $MCRegionPartPosition{$part});
        my ($start, $end) = split (/-/, $range);
        $length = $length + ($end - $start + 1);
        printf "<A HREF=\"/cgi-bin/hgTracks?db=%s&amp;encodeRegionMercator=full&amp;position=%s&amp;netHg16=full&amp;refGene=pack\" TARGET=browser2>%s</A> &nbsp;", $genome, $MCRegionPartPosition{$part}, $chr;
    }
    my $diff = $length - $LOlength;
    $diff = $diff/1000;
    $length = $length/1000;
    $length = $length/1000;
    printf "<TD>%.1f</TR>\n", $length;
    printf "<TD ALIGN=right>";
    if ($diff > 100 || $diff < -100) {
        printf("<FONT COLOR=red>");
    } elsif ($diff > 20 || $diff < -20) {
        printf("<FONT COLOR=orange>");
    }
    printf "%d</TR>\n", $diff;
    printf("</FONT>");
}

# end table

print "</table>\n";


