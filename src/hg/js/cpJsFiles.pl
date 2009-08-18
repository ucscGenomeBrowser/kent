#!/usr/bin/env perl

# Copy files to javascript directory; make versioned soft-links as appropriate (and delete obsolete ones too).

# $Id: cpJsFiles.pl,v 1.3 2009/08/13 16:25:31 markd Exp $

use strict;

use Getopt::Long;

sub usage
{
    my ($msg) = @_;
    print STDERR <<END;
$msg

Usage:

cpJsFiles.pl [-exclude=...] -destDir=... files
END
}

my ($exclude, $destDir, $debug);
my %exclude;

GetOptions("exclude=s" => \$exclude, "destDir=s" => \$destDir, "debug" => \$debug);

if($exclude) {
    %exclude = map { $_ => 1} split(/\s*,\s*/, $exclude);
}

usage("Missing/invalid destDir '$destDir'") if(!$destDir || !(-d $destDir));

$destDir =~ s/\/$//;

my @destFiles;
for (`ls $destDir`) {
    chomp;
    push(@destFiles, $_);
}

for my $file (@ARGV)
{
    if(!$exclude{$file}) {
        my @stat = stat($file) or die "Couldn't stat '$file'; err: $!";
        my $mtime = $stat[9];
        
        # update destination file as appropriate
        my $update = 0;
        my $destFile = "$destDir/$file";
        if(-e $destFile) {
            my @destStat = stat("$destFile") or die "Couldn't stat '$destFile'; err: $!";
            $update = ($destStat[9] < $mtime);
        } else {
            $update = 1;
        }
        if($update) {
            if (-e $destFile) {
                unlink($destFile) || die "Couldn't unlink $destFile'; err: $!";
            }
            !system("cp -p $file $destFile") || die "Couldn't cp $file to $destFile: err: $!";
        }

        if($file =~ /(.+)\.js$/) {
            my $prefix = $1;
            # make sure time is right, in case file; file might have been newer,
            # speculation that cp -p silently failed if user doesn't own destDir
            @stat = stat("$destFile") or die "Couldn't stat '$destFile'; err: $!";
            $mtime = $stat[9];

            my $softLink = $file;
            $softLink =~ s/\.js$/-$mtime.js/;

            # Delete obsolete symlinks
            for my $f (@destFiles) {
                if($f =~ /$prefix-(\d+)\.js/) {
                    if($f ne $softLink) {
                        print STDERR "Deleting old soft-link $f\n" if($debug);
                        unlink("$destDir/$f") || die "Couldn't unlink '$destDir/$softLink'; err: $!";
                    }
                }
            }
            # create new symlink
            if(!(-l "$destDir/$softLink")) {
                print STDERR "ln -s $destDir/$softLink\n" if($debug);
                !system("ln -s $destDir/$file $destDir/$softLink") || die "Couldn't ln -s $file; err: $!";
            }
        }
    }
}
