#!/usr/bin/env perl

# Copy files to javascript directory; make versioned soft-links as appropriate (and delete obsolete ones too).

# $Id: cpJsFiles.pl,v 1.2 2009/08/10 21:14:20 larrym Exp $

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
        if(-e "$destDir/$file") {
            my @destStat = stat("$destDir/$file") or die "Couldn't stat '$destDir/$file'; err: $!";
            $update = $destStat[9] < $mtime;
        } else {
            $update = 1;
        }
        if($update) {
            if(system("cp -p $file $destDir")) {
                # cp -p doesn't work if user doesn't own destDir, so fall-back to rm/cp
                !system("rm $destDir/$file") || die "Couldn't unlink $destDir/$file: err: $!";
                !system("cp $file $destDir") || die "Couldn't cp $file: err: $!";
            }
        }

        if($file =~ /(.+)\.js$/) {
            my $prefix = $1;
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
