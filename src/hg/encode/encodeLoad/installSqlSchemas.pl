#!/usr/bin/env perl

# copy .sql and .as files listed in @Encode::extendedTypes to $Encode::sqlCreate

use warnings;
use strict;

use File::stat;
use lib "/cluster/bin/scripts";
use Encode;

# extendedTypes types need .sql files
for my $type (@Encode::extendedTypes) {
    my $file = "$ENV{HOME}/kent/src/hg/lib/encode/${type}.sql";
    if(!(-e $file)) {
        $file = "$ENV{HOME}/kent/src/hg/lib/${type}.sql";
    }
    if(-e $file) {
        my $replace = 1;
        my $target = "$Encode::sqlCreate/$type.sql";
        if(-e $target) {
            my $fileStat = stat($file);
            my $targetStat = stat($target);
            $replace = $fileStat->mtime > $targetStat->mtime;
        }
        my $cmd = "cp $file $target.tmp";
        !system($cmd) || die  "system '$cmd' failed: $?";
        $cmd = "mv -f $target.tmp $target";
        !system($cmd) || die  "system '$cmd' failed: $?";
    } else {
        die "can't find sql file for type '$type'";
    }
}

