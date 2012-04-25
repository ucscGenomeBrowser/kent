#!/usr/bin/env perl

# copy .sql and .as files listed in @Encode::extendedTypes to $Encode::sqlCreate

use warnings;
use strict;

#use File::stat;
use lib "/cluster/bin/scripts";
use Encode;

my @types = ("bedLogR", "bedRrbs", "bedRnaElements", "narrowPeak", "gappedPeak", "broadPeak");

sub installFile {
    my ($sourceFile, $targetFile) = @_;
    my $replace = 1;
    if(-e $targetFile) {
        my $fileStat = (stat($sourceFile))[9];
        my $targetStat = (stat($targetFile))[9];
        $replace = ($fileStat > $targetStat);
    }
    if ($replace) {
        my $cmd = "cp $sourceFile $targetFile.tmp";
        !system($cmd) || die  "system '$cmd' failed: $?";
        $cmd = "mv -f $targetFile.tmp $targetFile";
        !system($cmd) || die  "system '$cmd' failed: $?";

    }
}

my $targetDir = $ARGV[0];
my $sourceDir = "$ENV{HOME}/kent/src/hg/lib/encode";
unless ($targetDir) {
    die "please specify a target directory\n";

}

my @localerrors;
# extendedTypes types need .sql files
for my $type (@types) {
    my $sql = "$sourceDir/${type}.sql";
    my $as = "$sourceDir/${type}.as";
    my $error = 0;
    unless (-e $as) {
        $error = 1;
        push @localerrors, "Missing .as file for type '$type'";
    }
    unless (-e $sql) {
        $error = 1;
        push @localerrors, "Missing .sql file for type '$type'";
    }
    if ($error) {
        next;
    } else {
        &installFile($as, "$targetDir/$type.as");
        &installFile($sql, "$targetDir/$type.sql");
    }
}

if (@localerrors) {
    print "Errors:\n";
    foreach my $error (@localerrors) {
        print "$error\n";
    }
}

