#!/usr/bin/env perl

# Copy files to javascript directory; make versioned soft-links as appropriate (and delete obsolete ones too).

# $Id: cpJsFiles.pl,v 1.4 2010/02/28 00:08:14 larrym Exp $

use strict;

use Cwd;
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

# To avoid screwing up mirrors who run "rsync --links", we chdir into the javascript directory and make non-absolute softlinks there.
my $cwd = getcwd();
chdir($destDir) || die "Couldn't chdir into '$destDir'; err: $!";

for my $file (@ARGV)
{
    if(!$exclude{$file}) {
        my @stat = stat("$cwd/$file") or die "Couldn't stat '$file'; err: $!";
        my $mtime = $stat[9];
        
        # update destination file as appropriate
        my $update = 0;
        my $destFile = $file;
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
            !system("cp -p $cwd/$file $destFile") || die "Couldn't cp $cwd/file to $destFile: err: $!";
        }

        if($file =~ /(.+)\.js$/) {
            my $prefix = $1;
            # make sure time is right, in case file; file might have been newer,
            # speculation that cp -p silently failed if user doesn't own destDir
            @stat = stat($destFile) or die "Couldn't stat '$destFile'; err: $!";
            $mtime = $stat[9];

            my $softLink = $file;
            $softLink =~ s/\.js$/-$mtime.js/;
            # Delete obsolete symlinks
            for my $f (@destFiles) {
                if($f =~ /^$prefix-(\d+)\.js$/) {
                    if($f ne $softLink) {
                        print STDERR "Deleting old soft-link $f\n" if($debug);
                        unlink($f) || die "Couldn't unlink obsolete softlink '$softLink'; err: $!";
                    }
                }
            }
            # create new symlink
            if(!(-l "$softLink")) {
                print STDERR "ln -s $softLink\n" if($debug);
                !system("ln -s $file $softLink") || die "Couldn't ln -s $file; err: $!";
            }
        }
    }
}
