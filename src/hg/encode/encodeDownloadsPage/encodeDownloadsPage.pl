#!/usr/bin/env perl

# hgEncodeDownloads.pl - generates a webpage for all tgz downloadable files in a directory.
#                        The first portion of the file name (delimieted by '.') should be the
#                        corresponding tableName in order to look up the dateReleased in trackDb.
#                        Called by automated submission pipeline
#
# $Header: /projects/compbio/cvsroot/kent/src/hg/encode/encodeDownloadsPage/encodeDownloadsPage.pl,v 1.21 2009/11/02 19:48:55 braney Exp $

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
    /;

sub usage {
    print STDERR <<END;
usage: hgEncodeDownloads.pl {index.html} [downloads-dir]

Creates the an HTML page listing the downloads in the current directory or optional directory

options:
    -help               Displays this usage info
    -preamble=file      File containing introductory information (written in HTML) that will be included in this file (default preamble.html)
    -db=hg18            Use a database other than the default hg18 (For aquiring releaseDate and metadata from trackDb)
    -fileType=mask	    mask for file types included (default '*.gz')
    -verbose=num        Set verbose level to num (default 1).

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
    print OUT_FILE " </HEAD>\n";
    print OUT_FILE " <BODY>\n";
    print OUT_FILE "<IMG SRC=\"/icons/back.gif\" ALT=\"[DIR]\"> <A HREF=\"../\">Parent Directory</A><BR>\n\n";
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

sub htmlEndPage {
# prints closing of the html downloads index.html file

    local *OUT_FILE = shift;

    print OUT_FILE "\n<HR>\n";
    print OUT_FILE "</BODY></HTML>\n";
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

# prints a row closing of an html table
sub sortableHtmlRow {
# returns a table row ready to print but prepended by sortable columns
    my ($sortablesRef,$file,$size,$date,$releaseDate,$metaData) = @_;

    my @sortables  = @{$sortablesRef};
    my $row =join(" ", @sortables);
    $row .= "|";

    if($releaseDate && length($releaseDate) > 0) {
        $row .= "<TR valign='top'><TD align='left'>&nbsp;&nbsp;$releaseDate";
    } else {
        $row .= "<TR valign='top'><TD align='left'>&nbsp;";
    }
    $row .= "<TD><A type='application/zip' target='_blank' HREF=\"$file\">$file</A>";
    $row .= "<TD align='right'>&nbsp;&nbsp;$size<TD>&nbsp;&nbsp;$date&nbsp;&nbsp;";
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
# Joins metadata tags and vals arrays into single array of tag=val
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

sub sortablesSetFromMetadataArrays {
# Joins metadata tags and vals arrays into single array of tag=val
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

my $ok = GetOptions("fileType=s",
                    "preamble=s",
                    "db=s",
                    "verbose=i",
                    );
usage() if (!$ok);
$opt_verbose = 1 if (!defined $opt_verbose);
my $fileMask = "*.gz";
   $fileMask = $opt_fileType if(defined $opt_fileType);

my $preamble = "preamble.html";
   $preamble = $opt_preamble if(defined $opt_preamble);

usage() if (scalar(@ARGV) < 1);

# Get command-line args
my $indexHtml = $ARGV[0];
my $downloadsDir = cwd();
$downloadsDir = $ARGV[1] if (scalar(@ARGV) > 1);

# Now find some files
my @fileList = `ls -hogL  --time-style=long-iso $downloadsDir/$fileMask 2> /dev/null`;
# -rw-rw-r--  1 101M 2008-10-27 14:34 /usr/local/apache/htdocs/goldenPath/hg18/wgEncodeYaleChIPseq/wgEncodeYaleChIPseqSignalK562Pol2.wig.gz
if(length(@fileList) == 0) {
    die ("ERROR; No files were found in \'$downloadsDir\' that match \'$fileMask\'.\n");
}

# TODO determine whether we should even look this up
$opt_db = "hg18" if(!defined $opt_db);
my $db = HgDb->new(DB => $opt_db);

open( OUT_FILE, "> $downloadsDir/$indexHtml") || die "SYS ERROR: Can't write to \'$downloadsDir/$indexHtml\' file; error: $!\n";
#print OUT_FILE @fileList;


my @readme;
if(open(README, "$downloadsDir/README.txt")) {
    @readme = <README>;
    close(README);
}

# Start the page
htmlStartPage(*OUT_FILE,$downloadsDir,$indexHtml,$preamble);

my @rows; #  Gather rows to be printed here

print OUT_FILE "<TABLE cellspacing=0>\n";
print OUT_FILE "<TR valign='bottom'><TH>RESTRICTED<BR>until<TH align='left'>&nbsp;&nbsp;File<TH align='right'>Size<TH>Submitted<TH align='left'>&nbsp;&nbsp;Details</TR>\n";

for my $line (@fileList) {
    chomp $line;
    my @file = split(/\s+/, $line);      # White space
#print OUT_FILE join ' ', @file,"\n";
    my @path = split('/', $file[5]);    # split on /
    my $fileName = $path[$#path]; # Last element
    my $tableName = "";
    my $dataType  = "";
    ### Special case for Elnitski BiPs !!!
    my $database = "hg18"; # default
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
    my @sortFields = ("cell","dataType","rnaExtract","localization","fragSize","mapAlgorithm","ripAntibody","ripTgtProtein","treatment","antibody","lab","type","view","level","annotation","replicate","subId");
    my @sortables = map( "~", (1..scalar(@sortFields))); # just has to have a tilde for each field
    my $typePrefix = "";
    my $results = $db->quickQuery("select type from $database.trackDb where tableName = '$tableName'");
    if($results) {
        my ($type) = split(/\s+/, $results);    # White space
        $metaData{type} = $type;
    }
    if(!$metaData{type}) {
        $metaData{type} = $dataType;
    }
    $results = $db->quickQuery("select settings from $database.trackDb where tableName = '$tableName'");
    if(!$results) {
        ### TODO: This needs to be replaced with a select from a fileDb table
        if(stat("fileDb.ra")) {
            $results = `grep metadata fileDb.ra | grep '$fileName'`;
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
        ### TODO: This needs to be replaced with a select from a fileDb table
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
                while($tags[$tix]) {
                    if($tags[$tix] eq "dateUnrestricted") {
                        $releaseDate = $vals[$tix];
                    } elsif($tags[$tix] eq "dateSubmitted") {
                        $submitDate = $vals[$tix];
                    }
                    $tix++;
                }
                if($metaData{type}) {
                    unshift @tags, "type"; # push values onto the front
                    unshift @vals, $metaData{type};
                }
                my %remove; # Don't display these metadata values
                $remove{project} = $remove{composite} = $remove{fileName} = $remove{dateSubmitted} = $remove{dateUnrestricted} = $remove{parentTable} = 1;
                ( $tagRef, $valRef ) = metadataArraysRemoveHash( \@tags,\@vals,\%remove );
                ( $tagRef, $valRef ) = metadataArraysMoveValuesToFront($tagRef, $valRef,\@sortFields);
                @tags = @{$tagRef};
                @vals = @{$valRef};

                @sortables = sortablesSetFromMetadataArrays( \@sortables,\@sortFields,\@tags,\@vals );
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
    push @rows, sortableHtmlRow(\@sortables,$fileName,$file[2],$submitDate,$releaseDate,$details);
}
sortAndPrintHtmlTableRows(*OUT_FILE,@rows);
print OUT_FILE "</TABLE>\n";

htmlEndPage(*OUT_FILE);

chdir $downloadsDir;
chmod 0775, $indexHtml;

exit 0;
