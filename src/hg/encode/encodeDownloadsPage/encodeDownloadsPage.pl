#!/usr/bin/env perl

# encodeDownloadsPage.pl - generates a webpage for all tgz downloadable files in a directory.
#                          The first portion of the file name (delimieted by '.') should be the
#                          corresponding tableName in order to look up the dateReleased in trackDb.
#                          Called by automated submission pipeline
#
# $Header: /projects/compbio/cvsroot/kent/src/hg/encode/encodeDownloadsPage/encodeDownloadsPage.pl,v 1.38 2010/04/27 22:59:03 tdreszer Exp $

use warnings;
use strict;

use File::stat;
use File::Basename;
use Getopt::Long;
use Time::Local;
use English;
use Carp qw(cluck);
use Cwd;
use IO::File;
use File::Basename;

use lib "/cluster/bin/scripts";
use Encode;
use HgAutomate;
use HgDb;

use vars qw/
    $opt_fileType
    $opt_preamble
    $opt_db
    $opt_verbose
    $opt_checksum
    $opt_redirect
    $opt_noMdb
    $opt_mdb
    $opt_sortable
    /;

our $checksumFile = "md5sum.txt";
our $textFile = "files.txt";    # plain text file list with metadata

sub usage {
    print STDERR <<END;
usage: encodeDownloadsPage.pl {index.html} [downloads-dir]

Creates an HTML page and README text file listing the downloads in the current directory or optional directory.

options:
    -db=hg19            Use a database other than the default hg19 (For aquiring releaseDate and metadata from trackDb).
    -redirect           HTML page will redirect to hgFileUi but only if 'fileSortOrder' exists in trackDb.
    -sortable           The HTML output will include jquery and utils.js javascript to allow sorting of the table (TESTING FEATURE).
    -checksum           Generate checksum file.
    -mdb=tableName      Use an explicit mdb table (e.g. 'mdb_braney') and don't fall back on trackDb/fileDb.ra methods.
    -preamble=file      File containing introductory information (in HTML) that will be included in this file (default preamble.html).
    -help               Displays this usage info.
    -verbose=num        Set verbose level to num (default 1).
  Historical options:
    -noMdb              Don't use an 'mdb' metadata table.  Instead, use the old trackDb/fileDb.ra methods.
    -fileType=mask      mask for file types included (default '*.gz').

END
exit 1;
}

sub htmlStartPage {
# prints the opening of the html downloads index.html file
    #local *OUT_FILE = shift;
    local *OUT_FILE = shift;

    my ($filePath,$fileName,$preamble) = @_;
    my ($volume,$directories,$file) = File::Spec->splitpath( $filePath, 1 );
    my @dirs = File::Spec->splitdir( $directories );

    print OUT_FILE "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">\n";
    print OUT_FILE "<HTML>\n";
    print OUT_FILE " <HEAD>\n";
    print OUT_FILE "  <TITLE>Index of $dirs[-1]</TITLE>\n";
    print OUT_FILE "    <META http-equiv='Content-Script-Type' content='text/javascript'>\n";
    print OUT_FILE "    <LINK rel='STYLESHEET' href='http://hgwdev.cse.ucsc.edu/style/HGStyle.css' TYPE='text/css' />\n";
    print OUT_FILE " </HEAD>\n";
    print OUT_FILE " <BODY>\n";
#   print OUT_FILE "<IMG SRC='http://<!--#echo var=\"BROWSER_HOST\" -->/icons/back.gif' ALT='[DIR]'> <A HREF='javascript:history.back();'>Parent Directory</A><BR>\n\n";
    print OUT_FILE "<IMG SRC='http://<!--#echo var=\"BROWSER_HOST\" -->/icons/back.gif' ALT='[DIR]'> <A HREF='../'>Parent Directory</A><BR>\n\n";
# The following doesn't do any good because we want to sort, not drag and drop!
#    # my ($second, $minute, $hour, $dayOfMonth, $month, $yearOffset, $dayOfWeek, $dayOfYear, $daylightSavings) = localtime();
#    my (undef, $minute, $hour, $dayOfMonth, undef, undef, undef, $dayOfYear, undef) = localtime();
#    print OUT_FILE "<script type='text/javascript' src='../js/jquery.js?v=" . $dayOfYear . $dayOfMonth . $hour . $minute . "'></script>\n";
#    print OUT_FILE "<noscript><b>Your browser does not support JavaScript so some functionality may be missing!</b></noscript>\n";
#    print OUT_FILE "<script type='text/javascript' src='../js/jquery.tablednd.js?v=" . $dayOfYear . $dayOfMonth . $hour . $minute . "'></script>\n";

    my $hasPreamble = 0;
    if($preamble && length($preamble) > 0) { # TODO check that prolog exists
        if(open( PREAMBLE, "$filePath/$preamble")) {
            $hasPreamble = 1;
            print OUT_FILE <PREAMBLE>;
            close(PREAMBLE);
        }
    }
    if(!$hasPreamble) {
        print OUT_FILE "<p>This directory contains data files available for download.</p>\n\n";
    }
    print OUT_FILE "<HR>\n\n";
}

sub htmlTrHeader {
# prints tr header row for sortable table
    local *OUT_FILE = shift;

    my ($colListRef,,$tagsRef,$valsRef) = @_;
    my @colList  = @{$colListRef};
    my $sorColCount = scalar(@colList);

    print OUT_FILE "<thead><TR valign='bottom'>\n";
    print OUT_FILE "<TH class='sortable sort". ($sorColCount + 2) ."'>RESTRICTED<BR>until</th>\n";
    print OUT_FILE "<TH class='sortable sort". ($sorColCount + 1) ."' align='left'>File</th>\n";
    print OUT_FILE "<TH class='sortable sort". ($sorColCount + 4) ."' align='right'>Size</th>\n";
    print OUT_FILE "<TH class='sortable sort". ($sorColCount + 3) ."'>Submitted</th>\n";
    my $cix = 0; # keep order
    while($colList[$cix]) {
        print OUT_FILE "<TH class='sortable sort". ($cix + 1) ."'>" . $colList[$cix] . "</th>\n";
        $cix++;
    }
    print OUT_FILE "<TH align='left'>&nbsp;&nbsp;Additional details</th>\n";
    print OUT_FILE "</TR></thead><tbody style='display:none;'>\n";
}

sub htmlEndPage {
# prints closing of the html downloads index.html file

    local *OUT_FILE = shift;

    print OUT_FILE "\n<HR>\n";
    print OUT_FILE @_;
    print OUT_FILE "</BODY></HTML>\n";
}

sub htmlRedirectPage {
# prints redirect page if compoiste has a fileSortOrder
    local *OUT_FILE = shift;

    my ($db, $database, $composite) = @_;

    my $results = $db->quickQuery("select settings from $database.trackDb where tableName = '$composite'");
    if($results) {
        my @settings = split(/\n/, $results); # New Line
        while (@settings) {
            my @pair = split(/\s+/, (shift @settings),2);
            if($pair[0] eq "fileSortOrder") { # fileSortOrderExists so di the cool thing and redirect
                print OUT_FILE "<!DOCTYPE HTML PUBLIC '-//W3C//DTD HTML 4.01 Transitional//EN' 'http://www.w3.org/TR/html4/loose.dtd'>\n";
                print OUT_FILE "<HTML>\n";
                print OUT_FILE "<HEAD>\n";
                print OUT_FILE "<META HTTP-EQUIV='Content-Type' CONTENT='text/html;CHARSET=iso-8859-1'>\n";
                print OUT_FILE "<META http-equiv='Content-Script-Type' content='text/javascript'>\n";
                print OUT_FILE "<META HTTP-EQUIV='Pragma' CONTENT='no-cache'>\n";
                print OUT_FILE "<META HTTP-EQUIV='Expires' CONTENT='-1'>\n";
                print OUT_FILE "<META http-equiv='refresh' content='0; URL=http://<!--#echo var=\"BROWSER_HOST\" -->/cgi-bin/hgFileUi?db=$database&g=$composite'>\n";
                print OUT_FILE "<TITLE>Redirecting...</TITLE>\n";
                print OUT_FILE "<LINK rel='STYLESHEET' href='../style/HGStyle-1304985632.css' TYPE='text/css' />\n";
                print OUT_FILE "</HEAD>\n\n";
                print OUT_FILE "<body onload=\"window.location.href='http://<!--#echo var=\"BROWSER_HOST\" -->/cgi-bin/hgFileUi?db=$database&g=$composite';\" BGCOLOR='#FFF9D2' LINK='#0000CC' VLINK='#330066' ALINK='#6600FF' text='#000000'>\n";
                print OUT_FILE "<p>Loading <a href='http://<!--#echo var=\"BROWSER_HOST\" -->/cgi-bin/hgFileUi?db=$database&g=$composite'>redirection target</a></p>\n";
                print OUT_FILE "<p>In approx. 1 second the redirection target page should load.<br>\n";
                print OUT_FILE "If it doesn't please select the link above.</p>\n";
                print OUT_FILE "</BODY>\n";
                print OUT_FILE "</HTML>\n";

                return 1;
            }
        }
    }
    return 0;
}

sub cmpRows {
# Compares the sortable fields of a pair of rows
    my ($aSortables,undef) = split(/\|/,$a,2);
    my ($bSortables,undef) = split(/\|/,$b,2);
    my @As = split(/ /,$aSortables);
    my @Bs = split(/ /,$bSortables);
    my $ret = 0;
    my $ix=0;
    while($ret == 0 && $As[$ix] && $Bs[$ix]) {
        $ret = ( $As[$ix] cmp $Bs[$ix] );
        $ix++;
    }
    return $ret;
}

sub sortAndPrintHtmlTableRows {
    local *OUT_FILE = shift;
    my @rows = sort cmpRows @_;
    while(my $line = shift @rows) {
        my ( undef,$row ) = split(/\|/, $line,2);
        print OUT_FILE $row;
    }
}

# prints a row of the html table
sub sortableHtmlRow {
# returns a table row ready to print but prepended by sortable columns
    my ($sortablesRef,$file,$size,$date,$releaseDate,$metaData) = @_;

    my @sortables  = @{$sortablesRef};
    my $row =join(" ", @sortables);
    $row .= "|";

    if($releaseDate && length($releaseDate) > 0) {
        $row .= "<TR valign='top'><TD align='left' nowrap>&nbsp;&nbsp;$releaseDate";
    } else {
        $row .= "<TR valign='top'><TD align='left'>&nbsp;";
    }
    $row .= "<TD><A type='application/zip' target='_blank' HREF=\"$file\">$file</A>";
    $row .= "<TD align='right' nowrap>&nbsp;&nbsp;$size<TD nowrap>&nbsp;&nbsp;$date&nbsp;&nbsp;";
    if($metaData && length($metaData) > 0) {
        $row .= "<TD nowrap>$metaData";
        # Find cell,dataType,antibody,lab,type,subId
        # Alternative would be to package up all of the javascript sort code and then make individual columns for sortable metadata
    }
    $row .= "</TR>\n";
}

sub sortableHtmlRowWithColumns {
# returns a table row ready to print but prepended by sortable columns
    my ($sortablesRef,$file,$size,$date,$releaseDate,$metaData,$colListValsRef) = @_;

    my @sortables  = @{$sortablesRef};
    my @colListVals  = @{$colListValsRef};
    my $row =join(" ", @sortables);
    $row .= "|";

    if($releaseDate && length($releaseDate) > 0) {
        $row .= "<TR valign='top'><TD align='left' nowrap>&nbsp;&nbsp;$releaseDate</TD>";
    } else {
        $row .= "<TR valign='top'><TD align='left'>&nbsp;</TD>";
    }
    $row .= "<TD><A type='application/zip' target='_blank' HREF=\"$file\">$file</A></TD>";
    $row .= "<TD align='right' nowrap>&nbsp;&nbsp;$size<TD nowrap>&nbsp;&nbsp;$date&nbsp;&nbsp;</TD>";
    my $cix = 0;
    while($colListVals[$cix]) {
        if($colListVals[$cix] ne "") {
            $row .= "<TD nowrap>$colListVals[$cix]</TD>";
        } else {
            $row .= "<TD nowrap>&nbsp;</TD>";
        }
        $cix++;
    }
    if($metaData && length($metaData) > 0) {
        $row .= "<TD nowrap>$metaData";
        # Find cell,dataType,antibody,lab,type,subId
        # Alternative would be to package up all of the javascript sort code and then make individual columns for sortable metadata
    }
    $row .= "</TR>\n";
}

sub htmlTableRow {
# prints a row closing of an html table
    local *OUT_FILE = shift;
    my ($file,$size,$date,$releaseDate,$metaData) = @_;

    if($releaseDate && length($releaseDate) > 0) {
        print OUT_FILE "<TR valign='top'><TD align='left'>&nbsp;&nbsp;$releaseDate\n";
    } else {
        print OUT_FILE "<TR valign='top'><TD align='left'>&nbsp;\n";
    }
    print OUT_FILE "<TD><A type='application/zip' target='_blank' HREF=\"$file\">$file</A>\n";
    print OUT_FILE "<TD align='right'>&nbsp;&nbsp;$size<TD>&nbsp;&nbsp;$date&nbsp;&nbsp;\n";
    if($metaData && length($metaData) > 0) {
        print OUT_FILE "<TD nowrap>$metaData\n";
    }
    print OUT_FILE "</TR>\n";
}

sub metadataArraysRemoveHash {
# Removes members of hash from metadata Arrays
    my ($tagsRef,$valsRef,$removeRef) = @_;
    my @tags = @{$tagsRef};
    my @vals = @{$valsRef};
    my %remove = %{$removeRef};

    my @tagsOk;
    my @valsOk;
    my $nix = 0;
    my $oix = 0;
    while($tags[$oix]) {
        if($remove{$tags[$oix]}) {
            $oix++;
        } else {
            $tagsOk[$nix  ] = $tags[$oix  ];
            $valsOk[$nix++] = $vals[$oix++];
        }
    }
    return ( \@tagsOk, \@valsOk );
}

sub metadataArraysMoveValuesToFront {
# Removes members of hash from metadata Arrays
    my ($tagsRef,$valsRef,$fieldsRef) = @_;
    my @tags = @{$tagsRef};
    my @vals = @{$valsRef};
    my @fields = @{$fieldsRef};
    my %fldsHash;
    my @labels;
    my @values;

    my $ix=0;
    while($fields[$ix]) {
        my $tix=0;
        while($tags[$tix]) {
            if($tags[$tix] eq $fields[$ix]) {
                push( @labels, $tags[$tix]);
                push( @values, $vals[$tix]);
            }
            $tix++;
        }
        $fldsHash{$fields[$ix++]} = $ix;
    }
    ( $tagsRef, $valsRef ) = metadataArraysRemoveHash( \@tags,\@vals,\%fldsHash );
    @tags = @{$tagsRef};
    @vals = @{$valsRef};
    push(@labels,@tags);
    push(@values,@vals);
    return ( \@labels, \@values );
}

sub sortablesSet {
# Updates a sortable list of values in fieldsRef order
    my ($sortablesRef,$fieldsRef,$tag,$val) = @_;
    my @sortables  = @{$sortablesRef};
    my @sortFields = @{$fieldsRef};
    my $six = 0; # keep order
    while($sortFields[$six]) {
        if($tag eq $sortFields[$six]) {
            if($val && $val ne "") {
                $sortables[$six] = $val;
            } else {
                $sortables[$six] = "~";  # must maintain field order during split on " "
            }
        }
        $six++;
    }
    return @sortables;
}

sub colListCreateFromSortables {
# Creates a list of columns from sortable sortable fiieds
    my ($sortablesRef,$fieldsRef) = @_;
    my @sortables  = @{$sortablesRef};
    my @sortFields = @{$fieldsRef};
    my @colList;
    my $six = 0;
    my $cix = 0; # keep order
    while($sortables[$six]) {
        if ($sortFields[$six] eq "dataVersion") {# Enough already... Should limit by list of sort approved columns
            return @colList;
        }
        if($sortables[$six] ne "" && $sortables[$six] ne "~") {
            $colList[$cix] = $sortFields[$six];
            $cix++;
        }
        $six++;
    }
    return @colList;
}

sub colListValsCreateFromMetadataArrays {
# Creates a list of columns from sortable sortable fiieds
    my ($colListRef,,$tagsRef,$valsRef) = @_;
    my @colList  = @{$colListRef};
    my @tags = @{$tagsRef};
    my @vals = @{$valsRef};
    my @colListVals;
    my $cix = 0; # keep order
    while($colList[$cix]) {
        my $mix = 0; # keep order
        while($tags[$mix]) {
            if($colList[$cix] eq $tags[$mix]) {
                $colListVals[$cix] = $vals[$mix];
            }
            $mix++;
        }
        if (!$colListVals[$cix] || $colListVals[$cix] eq "") {
            $colListVals[$cix] = "&nbsp;";
        }
        $cix++;
    }
    return @colListVals;
}

sub sortablesSetFromMetadataArrays {
# updates a sorables list from metadata arrays
    my ($sortablesRef,$fieldsRef,$tagsRef,$valsRef) = @_;
    my @sortables  = @{$sortablesRef};
    #my @sortFields = @{$fieldsRef};
    my @tags = @{$tagsRef};
    my @vals = @{$valsRef};
    my $tix = 0;
    while($tags[$tix]) {
        @sortables = sortablesSet( \@sortables,$fieldsRef,$tags[$tix],$vals[$tix]);
        $tix++;
    }
    return @sortables;
}

sub metadataArraysJoin {
# Joins metadata tags and vals arrays into single array of tag=val
    my ($tagsRef,$valsRef) = @_;
    my @tags = @{$tagsRef};
    my @vals = @{$valsRef};
    my @meta;
    my $tix = 0;
    while($tags[$tix]) {
        $meta[$tix] = join("=",($tags[$tix],$vals[$tix]));
        $tix++;
    }
    return @meta;
}

############################################################################
# Main

# Get ls -hog of directory ( working dir or -dir )
  #ls -hog --time-style=long-iso *.gz
  #-rw-rw-r--  1 101M 2008-10-27 14:34 wgEncodeYaleChIPseqSignalK562Pol2.wig.gz

my $now = time();

my $ok = GetOptions("checksum",
                    "redirect",
                    "fileType=s",
                    "db=s",
                    "mdb=s",
                    "noMdb",
                    "preamble=s",
                    "sortable",
                    "verbose=i",
                    );
usage() if (!$ok);
$opt_verbose = 1 if (!defined $opt_verbose);
my $fileMask = "*.gz *.bb *.bigWig *.bam *.bai *.tgz";
   $fileMask = $opt_fileType if(defined $opt_fileType);

my $preamble = "preamble.html";
   $preamble = $opt_preamble if(defined $opt_preamble);

my $mdb = "metaDb";
   $mdb = $opt_mdb if(defined $opt_mdb);

usage() if (scalar(@ARGV) < 1);

# Get command-line args
my $indexHtml = $ARGV[0];
my $downloadsDir = cwd();
$downloadsDir = $ARGV[1] if (scalar(@ARGV) > 1);

# Now find some files
#my @fileList = `ls -hogL  --time-style=long-iso $downloadsDir/$fileMask 2> /dev/null`;
my @fileList = `cd $downloadsDir;ls -hogL  --time-style=long-iso $fileMask 2> /dev/null`;
# -rw-rw-r--  1 101M 2008-10-27 14:34 /usr/local/apache/htdocs/goldenPath/hg18/wgEncodeYaleChIPseq/wgEncodeYaleChIPseqSignalK562Pol2.wig.gz
if(length(@fileList) == 0) {
    die ("ERROR; No files were found in \'$downloadsDir\' that match \'$fileMask\'.\n");
}
my @files;      # Stripped down to filenames, for checksumming

# TODO determine whether we should even look this up
$opt_db = "hg19" if(!defined $opt_db);
my $db = HgDb->new(DB => $opt_db);

open( OUT_FILE, "> $downloadsDir/$indexHtml") || die "SYS ERROR: Can't write to \'$downloadsDir/$indexHtml\' file; error: $!\n";
if($indexHtml eq "index.html") {  # Only make this text file when making index.html
    open( TEXT_FILE, "> $downloadsDir/$textFile") || die "SYS ERROR: Can't write to \'$downloadsDir/$textFile\' file; error: $!\n";
}
#print OUT_FILE @fileList;


my @readme;
if(open(README, "$downloadsDir/README.txt")) {
    @readme = <README>;
    close(README);
}

my $composite = "unknown";  # The composite needs to be discovered

# NOTE: these two arrays should be collapsed, and HTML derived from plain text
my @rows; #  Gather rows to be printed here
my @textRows; #  Gather rows to be printed here (plain-text)
my $columnsDetermined = 0; # Only used by $opt_sortable
my @colList; # Only one colList made off the first row and declaring which are the metadata values to be stand alone columns

for my $line (@fileList) {
    chomp $line;
    my @file = split(/\s+/, $line);      # White space
#print OUT_FILE join ' ', @file,"\n";
    my @path = split('/', $file[5]);    # split on /
    my $fileName = $path[$#path]; # Last element
    push @files, $fileName;
    my $tableName = "";
    my $dataType  = "";
    ### Special case for Elnitski BiPs !!!
    my $database = "hg19"; # default
    if ( ($fileName =~ tr/\.//) == 3 ) {  # tableName.db.dataType.gz
        ($tableName,$database,$dataType) = split('\.', $fileName);   # tableName I hope
        if($database ne "panTro2" && $database ne "mm9" && $database ne "canFam2"
        && $database ne "rheMac2" && $database ne "rn4" && $database ne "bosTau4") {
            $database = "hg18"; # default
        }
    } else
    ### Special case for Elnitski BiPs !!!
    {
        ($tableName,$dataType) = split('\.', $fileName);   # tableName I hope
    }
#print OUT_FILE "Path:$file[5]  File:$fileName  Table:$tableName\n";

    my $releaseDate = "";
    my $submitDate = "";
    my %metaData;

    ### TODO: Developer: set sort order here; sortables must have same number of strings and '~' is lowest val printable
    #my @sortFields = ("cell","dataType","rnaExtract","localization","fragSize","mapAlgorithm","ripAntibody","ripTgtProtein","treatment","antibody","protocol","input","lab","type","view","level","annotation","rank","replicate","subId");
    # Complete sort order, (I hope)
    my @sortFields = ("species","assembly","dataType","cell","rnaExtract","localization","phase","treatment","antibody","protocol","ripAntibody",
                      "ripTgtProtein","restrictionEnzyme","promoter","control","replicate","expId","labExpId","setType","controlId","submittedDataVersion","subId","dataVersion",
                      "dateSubmitted","dateResubmitted","dateReloaded","dateUnrestricted","project","grant","lab","labVersion","softwareVersion",
                      "mapAlgorithm","fragSize","fragLength","medianFragmentLength","fragmentLengthRange","chromStart","view","type","composite",
                      "tableName","parentTable","fileName","dccInternalNotes");
    my @sortables = map( "~", (1..scalar(@sortFields))); # just has to have a tilde for each field
    my @colListVals;
    my $typePrefix = "";

    my $results = $db->quickQuery("select type from $database.trackDb where tableName = '$tableName'");
    if($results) {
        my ($type) = split(/\s+/, $results);    # White space
        $metaData{type} = $type;
    }
    if(!$metaData{type}) {
        $metaData{type} = $dataType;
    }

    $results = "";
    # Use the metaTbl for metadata
    if (!defined $opt_noMdb) {
        my $queryResults = $db->execute("select var,val from $mdb where obj = '$tableName'");
        if($queryResults) {
            my @pairVars;
            while(my @row = $queryResults->fetchrow_array()) {

                # FIXME: When trackDb metadata is no longer used, this routine should be replaced with more direct metaData loading
                #$metaData{$row[0]} = $row[1];
                if($row[0] ne 'objType') {
                    if($row[1] =~ m/\s/) { # Looking for a space in the string
                        push @pairVars, join('=',$row[0],'"' . $row[1] . '"' );
                    } else {
                        push @pairVars, join('=',$row[0],$row[1] );
                    }
                }
            }
            if(scalar(@pairVars) > 0) {
                $results = "metadata " . join(' ',@pairVars );
            }
        }
    }

    if(!defined $opt_mdb) {
        if(!$results) {
            $results = $db->quickQuery("select settings from $database.trackDb where tableName = '$tableName'");
        }
        if(!$results) {
            if(stat("fileDb.ra")) {
                $results = `grep metadata fileDb.ra | grep '$fileName' | tail -1`;  # Always prefer the last line found
                chomp $results;
                $results =~ s/^ +//;
                $results =~ s/ +$//;
            }
            if(!$results) {
                my $associatedTable = $tableName;
                $associatedTable =~ s/RawData/RawSignal/    if $tableName =~ /RawData/;
                $associatedTable =~ s/Alignments/RawSignal/ if $tableName =~ /Alignments/;
                if($tableName ne $associatedTable) {
                    $results = $db->quickQuery("select settings from trackDb where tableName = '$associatedTable'");
                    if($results) {
                        $metaData{parent} = "RawData&rarr;RawSignal" if $tableName =~ /RawData/;
                        $metaData{parent} = "Alignments&rarr;RawSignal" if $tableName =~ /Alignments/;
                    }
                }
            }
        }
    }
    if( $results ) {
        my @settings = split(/\n/, $results); # New Line
        while (@settings) {
            my @pair = split(/\s+/, (shift @settings),2);
            if($pair[0] eq "releaseDate" && length($releaseDate) == 0) {       # metadata has priority
                $releaseDate = $pair[1];
            } elsif($pair[0] eq "dateSubmitted" && length($submitDate) == 0) { # metadata has priority
                $submitDate = $pair[1];
            } elsif($pair[0] eq "cell" || $pair[0] eq "antibody" || $pair[0] eq "promoter") {
                $metaData{$pair[0]} = $pair[1];
                @sortables = sortablesSet( \@sortables,\@sortFields,$pair[0],$pair[1] );
            } elsif($pair[0] eq "metadata") {
                # Use metadata setting with priority
                my ( $tagRef, $valRef ) = Encode::metadataLineToArrays($pair[1]);
                my @tags = @{$tagRef};
                my @vals = @{$valRef};
                my $tix = 0;
                my $input = "";
                while($tags[$tix]) {
                    if($tags[$tix] eq "dateUnrestricted") {
                        $releaseDate = $vals[$tix];
                    } elsif($tags[$tix] eq "dateSubmitted") {
                        $submitDate = $vals[$tix];
                    } elsif($tags[$tix] eq "antibody" || $tags[$tix] eq "input") {
                        if($input eq "") {
                            $input = $vals[$tix];
                        } elsif($input eq $vals[$tix]) {
                            $input = "removeAntiBodyDup";
                        }
                    }
                    if ($composite eq "unknown" && $tags[$tix] eq "composite") {
                        $composite = $vals[$tix];
                    }
                    $tix++;
                }
                if ($fileName =~ m/bai$/) {
                    $metaData{type} = "bai";
                }
                if($metaData{type}) {
                    unshift @tags, "type"; # push values onto the front
                    unshift @vals, $metaData{type};
                }
                my %remove; # Don't display these metadata values
                $remove{tableName} = $remove{fileIndex} = $remove{project} = $remove{composite} = $remove{fileName} = 1;
                $remove{dccInternalNotes} = $remove{dateSubmitted} = $remove{dateUnrestricted} = $remove{parentTable} = 1;
                $remove{antibody} = 1 if($input eq "removeAntiBodyDup");  # remove antibody if input=antibody
                ( $tagRef, $valRef ) = metadataArraysRemoveHash( \@tags,\@vals,\%remove );
                ( $tagRef, $valRef ) = metadataArraysMoveValuesToFront($tagRef, $valRef,\@sortFields);
                @tags = @{$tagRef};
                @vals = @{$valRef};

                @sortables = sortablesSetFromMetadataArrays( \@sortables,\@sortFields,\@tags,\@vals );
                if (defined $opt_sortable) {
                    if (!$columnsDetermined) {
                        # Note this is done only on the first line.  So whatever unsorted line is first will determine the list of columns
                        @colList = colListCreateFromSortables( \@sortables,\@sortFields );
                        push @colList, "type"; # push values onto the front
                        $columnsDetermined = 1;
                    }
                    # create colListValues values from meta arrays and @colList
                    @colListVals = colListValsCreateFromMetadataArrays( \@colList,\@tags,\@vals );
                    # remove colList values from meta arrays
                    my %remove = map { $_ => 1 } @colList; # Create has of all colList members
                    ( $tagRef, $valRef ) = metadataArraysRemoveHash( \@tags,\@vals,\%remove );
                    my @leftOverTags = @{$tagRef};
                    my @leftOverVals = @{$valRef};
                    my @leftOverMetas = metadataArraysJoin( \@leftOverTags,\@leftOverVals );
                    $metaData{leftOver} = join("; ", @leftOverMetas);
                }
                my @metas = metadataArraysJoin( \@tags,\@vals );

                $metaData{metadata} = join("; ", @metas);
            }
        }
    } else { ### Obsoleted by metadata setting
        if($dataType eq "fastq" || $dataType eq "tagAlign" || $dataType eq "csfasta" || $dataType eq "csqual") {
            $metaData{type} = $dataType;
        }

        ### Obsoleted by metadata setting
        ### # pull metadata out of README.txt (if any is available).
        ### my $active = 0;
        ### for my $line (@readme) {
        ###     chomp $line;
        ###     if($line =~ /file: $fileName/) {
        ###         $active = 1;
        ###     } elsif($active) {
        ###         if($line =~ /(.*):(.*)/) {
        ###             my ($name, $var) = ($1, $2);
        ###             $metaData{$name} = $var if($name !~ /restricted/i);
        ###         } else {
        ###             last;
        ###         }
        ###     }
        ### }
    }
    if(length($submitDate) == 0) {
        $submitDate = $file[3];
    }
    if(length($releaseDate) == 0) {
        my ($YYYY,$MM,$DD) = split('-',$submitDate);
        $MM = $MM - 1;
        my (undef, undef, undef, $rMDay, $rMon, $rYear) = Encode::restrictionDate(timelocal(0,0,0,$DD,$MM,$YYYY));
        $rMon = $rMon + 1;
        $releaseDate = sprintf("%04d-%02d-%02d", (1900 + $rYear),$rMon,$rMDay);
    }
    my @tmp;
    if($metaData{metadata}) {
        if($metaData{parent}) {
            push(@tmp, $metaData{parent});
            delete $metaData{parent};
        }
        push(@tmp, $metaData{metadata});
    } else {
        if(my $type = $metaData{type}) {
            push(@tmp, "type=$type");
            delete $metaData{type};
        }
        if(my $cell = $metaData{cell}) {
            push(@tmp, "cell=$cell");
            delete $metaData{cell};
        }
        if($metaData{parent}) {
            push(@tmp, $metaData{parent});
            delete $metaData{parent};
        }
        push(@tmp, "$_=" . $metaData{$_}) for (sort keys %metaData);
    }
    my $details = join("; ", @tmp);

    # If releaseDate is in the past then don't bother showing it.
# QA wants dates all the time
#    if(length($releaseDate) > 0) {
#        my ($YYYY,$MM,$DD) = split('-',$releaseDate);
#        my $then = timegm(0,0,0,$DD,$MM-1,$YYYY);
#        my $now = time();
#        my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($now);
#        if($then -$now < 0) {
#            $releaseDate = "";
#        }
#    }

    #htmlTableRow(*OUT_FILE,$fileName,$file[2],$submitDate,$releaseDate,$details);
    if (defined $opt_sortable) {
        # And add @colListVals to the table rows
        push @rows, sortableHtmlRowWithColumns(\@sortables,$fileName,$file[2],$submitDate,$releaseDate,$metaData{leftOver},\@colListVals);
    } else {
        push @rows, sortableHtmlRow(\@sortables,$fileName,$file[2],$submitDate,$releaseDate,$details);
    }

    if($indexHtml eq "index.html") {  # Only make this text file when making index.html
        printf TEXT_FILE "%s\tsize=%s; dateSubmitted=%s; %s\n", $fileName, $file[2], $submitDate, $details;
    }
}

# Redirect if the CGI hgFileUi should be displayed
if (!defined $opt_redirect || htmlRedirectPage(*OUT_FILE,$db,$opt_db,$composite) == 0) {
    # Can't redirect to the hgFileUi CGI so make a page here

    # Start the page
    htmlStartPage(*OUT_FILE,$downloadsDir,$indexHtml,$preamble);

    if (defined $opt_sortable) {
        #print OUT_FILE "<script type='text/javascript' SRC='../js/jquery-1291074736.js'></script>\n";  Can't have timestamped version on static page (NOT A CGI)
        print OUT_FILE "<script type='text/javascript' SRC='http://hgwdev.cse.ucsc.edu/js/tdreszer/jquery.js'></script>\n";
        print OUT_FILE "<script type='text/javascript' SRC='http://hgwdev.cse.ucsc.edu/js/tdreszer/utils.js'></script>\n";
        print OUT_FILE "<TABLE class='sortable' style='border: 2px outset #006600;'>\n";

        htmlTrHeader(*OUT_FILE,\@colList);
    } else {
        print OUT_FILE "<TABLE cellspacing=0>\n";
        print OUT_FILE "<TR valign='bottom'><TH class'sortable'>RESTRICTED<BR>until<TH align='left' class'sortTh'>&nbsp;&nbsp;File<TH align='right' class'sortTh'>Size<TH class'sortTh'>Submitted<TH align='left' class'sortTh'>&nbsp;&nbsp;Details</TR>\n";
        print OUT_FILE "</TR>\n";
    }

    sortAndPrintHtmlTableRows(*OUT_FILE,@rows);
    if (defined $opt_sortable) {
        print OUT_FILE "</tbody></TABLE>\n";
        print OUT_FILE "<script type='text/javascript'>{\$(document).ready(function() {sortTableInitialize(\$('table.sortable')[0],true,true);});}</script>\n";
    } else {
        print OUT_FILE "</TABLE>\n";
    }

    my $conclusion = "<i>" . scalar(@rows) . " files</i>\n";

    htmlEndPage(*OUT_FILE,$conclusion);
}
if($indexHtml eq "index.html") {  # Only make this text file when making index.html
    close TEXT_FILE;
}

# create file of checksums
if (defined $opt_checksum) {
    open STDOUT, ">$checksumFile" or die "Cannot open $checksumFile file";
    foreach my $file (@files) {
        system("md5sum", $file);
    }
    close STDOUT;
}

chdir $downloadsDir;
chmod 0775, $indexHtml;
chmod 0664, $checksumFile;
chmod 0664, $textFile;

exit 0;
