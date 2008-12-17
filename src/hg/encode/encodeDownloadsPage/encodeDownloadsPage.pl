#!/usr/bin/env perl

# hgEncodeDownloads.pl - generates a webpage for all tgz downloadable files in a directory.
#                        The first portion of the file name (delimieted by '.') should be the
#                        corresponding tableName in order to look up the dateReleased in trackDb.
#                        Called by automated submission pipeline
#
# $Header: /projects/compbio/cvsroot/kent/src/hg/encode/encodeDownloadsPage/encodeDownloadsPage.pl,v 1.5 2008/12/02 22:09:12 tdreszer Exp $

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
#use RAFile;
#use SafePipe;

use vars qw/
    $opt_fileType
    $opt_preamble
    $opt_db
    $opt_verbose
    /;
#    $opt_timing

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

    print OUT_FILE "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">\n";
    print OUT_FILE "<HTML>\n";
    print OUT_FILE " <HEAD>\n";
    print OUT_FILE "  <TITLE>Index of $filePath</TITLE>\n";
    print OUT_FILE " </HEAD>\n";
    print OUT_FILE " <BODY>\n";
    print OUT_FILE "<IMG SRC=\"/icons/back.gif\" ALT=\"[DIR]\"> <A HREF=\"../\">Parent Directory</A><BR>\n\n";

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

sub htmlTableRow {
# prints a row closing of an html table
    local *OUT_FILE = shift;
    my ($file,$size,$date,$releaseDate,$metaData) = @_;

    if($releaseDate && length($releaseDate) > 0) {
        print OUT_FILE "<TR><TD align='left'>&nbsp;&nbsp;$releaseDate\n";
    } else {
        print OUT_FILE "<TR><TD align='left'>&nbsp;\n";
    }
    print OUT_FILE "<TD><A type='application/zip' target='_blank' HREF=\"$file\">$file</A>\n";
    print OUT_FILE "<TD align='right'>&nbsp;&nbsp;$size<TD>&nbsp;&nbsp;$date&nbsp;&nbsp;\n";
    if($metaData && length($metaData) > 0) {
        print OUT_FILE "<TD>$metaData\n";
    }
    print OUT_FILE "</TR>\n";
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
my $indexHtml = $ARGV[0];	# currently not used
my $downloadsDir = cwd();
   $downloadsDir = $ARGV[1] if (scalar(@ARGV) > 1);

# Now find some files
my @fileList = `ls -hog  --time-style=long-iso $downloadsDir/$fileMask 2> /dev/null`;
# -rw-rw-r--  1 101M 2008-10-27 14:34 /usr/local/apache/htdocs/goldenPath/hg18/wgEncodeYaleChIPseq/wgEncodeYaleChIPseqSignalK562Pol2.wig.gz
if(length(@fileList) == 0) {
    die ("ERROR; No files were found in \'$downloadsDir\' that match \'$fileMask\'.\n");
}

# TODO determine whether we should even look this up
$opt_db = "hg18" if(!defined $opt_db);
my $db = HgDb->new(DB => $opt_db);

open( OUT_FILE, "> $downloadsDir/$indexHtml") || die "SYS ERROR: Can't write to \'$downloadsDir/$indexHtml\' file; error: $!\n";
#print OUT_FILE @fileList;

# Start the page
htmlStartPage(*OUT_FILE,$downloadsDir,$indexHtml,$preamble);

print OUT_FILE "<TABLE cellspacing=0>\n";
print OUT_FILE "<TR valign='bottom'><TH>RESTRICTED<BR>until<TH align='left'>&nbsp;&nbsp;File<TH align='right'>Size<TH>Submitted<TH align='left'>&nbsp;&nbsp;Details</TR>\n";

for my $line (@fileList) {
    chomp $line;
    my @file = split(/\s+/, $line);      # White space
#print OUT_FILE join ' ', @file,"\n";
    my @path = split('/', $file[5]);    # split on /
    my $fileName = $path[$#path]; # Last element
    my ($tableName,$dataType) = split('\.', $fileName);   # tableName I hope
#print OUT_FILE "Path:$file[5]  File:$fileName  Table:$tableName\n";

    my $releaseDate = "";
    my $metaData = "";
    my $results = $db->quickQuery("select type from trackDb where tableName = '$tableName'");
    if($results) {
        my ($type) = split(/\s+/, $results);    # White space
        $metaData = "Type:$type ";
        $results = $db->quickQuery("select settings from trackDb where tableName = '$tableName'");
#print OUT_FILE "Settings:$results\n";
        if( $results ) {
            my @settings = split(/\n/, $results); # New Line
            while (@settings) {
                my @pair = split(/\s+/, (shift @settings),3);
#print OUT_FILE "Settings:". join('=',@pair) . "\n";
                if($pair[0] eq "subTrack") {
                    my $parentSettings = $db->quickQuery("select settings from trackDb where tableName = '$pair[1]'");
                    if( $parentSettings ) {
                        my @parent = split(/\n/, $parentSettings); # New Line
                        while (@parent) {
                            my @par = split(/\s+/, (shift @parent));
                            if( $par[0] eq "wgEncode" ) {
                                $metaData = "ENCODE $metaData";
                            }
                        }
                    }
                } elsif($pair[0] eq "releaseDate") {
                    $releaseDate = $pair[1];
                } elsif($pair[0] eq "dateSubmitted" && length($releaseDate) == 0) {
                    my ($YYYY,$MM,$DD) = split('-',$pair[1]);
                    $MM = $MM - 1;
                    my (undef, undef, undef, $rMDay, $rMon, $rYear) = Encode::restrictionDate(timelocal(0,0,0,$DD,$MM,$YYYY));
                    $rMon = $rMon + 1;
                    $releaseDate = sprintf("%04d-%02d-%02d", (1900 + $rYear),$rMon,$rMDay);
                } elsif($pair[0] eq "cell" || $pair[0] eq "antibody" || $pair[0] eq "promoter") {
                    $metaData .= "$pair[0]:$pair[1] ";
                }
            }
        }
    } else {
        if($dataType eq "fastq" || $dataType eq "tagAlign" || $dataType eq "csfasta" || $dataType eq "csqual") {
            $metaData = "ENCODE Type:$dataType";
            my ($YYYY,$MM,$DD) = split('-',$file[3]);
            $MM = $MM - 1;
            my (undef, undef, undef, $rMDay, $rMon, $rYear) = Encode::restrictionDate(timelocal(0,0,0,$DD,$MM,$YYYY));
            $rMon = $rMon + 1;
            $releaseDate = sprintf("%04d-%02d-%02d", (1900 + $rYear),$rMon,$rMDay);
        }
    }
    # Now file contains: [file without path] [tableName] [size] [date] [releaseDate] [fullpath/file]
    htmlTableRow(*OUT_FILE,$fileName,$file[2],$file[3],$releaseDate,$metaData);
}
print OUT_FILE "</TABLE>\n";

htmlEndPage(*OUT_FILE);

exit 0;
