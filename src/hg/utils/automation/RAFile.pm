#
# RAFile: methods to access and parse ra files
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/RAFile.pm instead.
#
# $Id: RAFile.pm,v 1.1 2008/04/10 20:08:16 larrym Exp $

package RAFile;

use warnings;
use strict;

use Carp;
use vars qw(@ISA @EXPORT_OK);
use Exporter;

@ISA = qw(Exporter);

@EXPORT_OK = (
    qw( readRaFile
      ),
);

sub readRaFile
{
# read an .ra file and return list of {KEYWORD => keyword, HASH => hash} annonymous hashes (keyword is keyword value of first 
# field of each ra entry); e.g.
#
# 	tablename wgEncodeDukeDnaseSignalHeLaS3
#	track wgEncodeDukeDnaseSignal
#
# => ({KEYWORD => 'wgEncodeDukeDnaseSignalHeLaS3', HASH => {tableName => 'wgEncodeDukeDnaseSignalHeLaS3', track => 'wgEncodeDukeDnaseSignal'})
    my ($file) = @_;

    my $inRecord = 0;
    my @ra = ();
    my $hash = {};
    my $keyword = "";
    open(FILE, $file) or die "Cannot open file: $file; error: $!";
    while(<FILE>) {
        chomp;
        my $line = $_;
        if($line !~ /^#/) {
           if(length($line) == 0) {
               if($inRecord) {
                   push(@ra, {KEYWORD => $keyword, HASH => $hash});
                   $hash = {};
               }
               $inRecord = 0;
           } else {
               if($line =~ /([^ ]+) (.*)/) {
                   my ($word, $value) = ($1, $2);
                   # print STDERR "$word\t$value\n";
                   if(!$inRecord) {
                       $inRecord = 1;
                       $keyword = $value;
                   }
                   $hash->{$word} = $value;
               } else {
                   die "syntax error: invalid line in .ra file: $line\n";
               }
           }
       }
    }
    if($inRecord) {
        push(@ra, {KEYWORD => $keyword, HASH => $hash});
    }
    close(FILE);
    return @ra;
}

1;
