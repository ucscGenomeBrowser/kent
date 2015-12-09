#!/usr/bin/env perl

# Copy files to resource directory; make versioned soft-links and/or files as appropriate (and delete obsolete ones too).

use strict;

use Cwd;
use Getopt::Long;

sub usage
{
    my ($msg) = @_;
    print STDERR <<END;
$msg

Usage:

cpAndLinkToResourceFiles.pl [OPTION] -destDir=dir -versionFile=file files

Copy any modified static files to a directory in the apache DocumentRoot and create appropriately versioned softlinks.

Options:

     -destDir=dir          Directory where we copy files
     -exclude=fileList     Comma-delimited list of files to exclude (useful if the files argument was created using "ls *.js")
     -force                Force update of all files
     -forceVersionNumbers  Force use of CGI versioned softlinks; useful for debugging in dev trees
     -minify               Minify javascript files (currently experimental)
     -versionFile=file     Get CGI version from this header file
END
}

my ($exclude, $destDir, $debug, $versionFile, $cgiVersion, $useMtimes);
my %exclude;
my %seen;
my $forceVersionNumbers;   # You can use this option to test CGI-versioned links on dev server
my ($force, $minify);

# minify code is experimental (hence this hardwired path; I'm not sure where to install the jar (in utils dir?)).
my $minifyJar = "/cluster/home/larrym/tmp/yuicompressor-2.4.6/build/yuicompressor-2.4.6.jar";

GetOptions("exclude=s" => \$exclude, "destDir=s" => \$destDir, "debug" => \$debug, "force" => \$force, "minify" => \$minify,
           "versionFile=s" => \$versionFile,  "forceVersionNumbers" =>  \$forceVersionNumbers);

if($exclude) {
    %exclude = map { $_ => 1} split(/\s*,\s*/, $exclude);
}

if(!$destDir || !(-d $destDir)) {
    usage "Missing/invalid destDir";
    die;
}

my $host = $ENV{HOST};
if(!defined($host)) {
    $host = `/bin/hostname`;
    chomp($host);
}

# Use version based copies in production sites, mtime based links in dev sites (see redmine #3170 and 5104).
if($forceVersionNumbers) {
    $useMtimes = 0;
} else {
    $useMtimes = $host eq 'hgwdev' || $host eq 'genome-test' || $host eq 'genome-preview';
}

if(!$useMtimes) {
    if(defined($versionFile)) {
        open(FILE, $versionFile) || die "Couldn't open '$versionFile'; err: $!";
        while(<FILE>) {
            if(/CGI_VERSION \"([^\"]+)\"/) {
                $cgiVersion = $1;
            }
        }
        close(FILE);
        if(!defined($cgiVersion)) {
            die "Couldn't find CGI_VERSION in '$versionFile'";
        }
    } else {
        die "Missing required versionFile parameter";
    }
}

$destDir =~ s/\/$//;

my @destFiles;
opendir(DIR, $destDir);
for my $file (readdir(DIR)) {
    if($file !~ /^\./) {
        push(@destFiles, $file);
    }
}
closedir(DIR);

# To avoid screwing up mirrors who run "rsync --links", we chdir into the resource directory and make non-absolute softlinks there.
my $cwd = getcwd();
chdir($destDir) || die "Couldn't chdir into '$destDir'; err: $!";

for my $file (@ARGV)
{
    if(!$seen{$file} && !$exclude{$file}) {
        $seen{$file} = 1;
        my $srcFile = "$cwd/$file";
        my @stat = stat($srcFile) or die "Couldn't stat '$srcFile'; err: $!";
        my $mtime = $stat[9];
        my ($prefix, $suffix);
        if($file =~ /(.+)\.([a-z]+)$/) {
            $prefix = $1;
            $suffix = $2;
        } else {
            die "Couldn't parse prefix and suffix out of file '$file'";
        }

        if($useMtimes) {
            # On dev machine, we copy file to apache dir and then create a mtime versioned softlink to this file.

            my $update = 0;
            my $exists = 0;
            my $destFile = $file;
            if(-e $destFile) {
                my @destStat = stat("$destFile") or die "Couldn't stat '$destFile'; err: $!";
                $update = ($destStat[9] < $mtime);
                $exists = 1;
            } else {
                $update = 1;
            }
            $update = $update || $force;
            if($update) {
                if ($exists) {
                    unlink($destFile) || die "Couldn't unlink $destFile'; err: $!";
                }
                if($debug) {
                    print STDERR "cp -p $srcFile $destFile\n";
                }
                !system("cp -p $srcFile $destFile") || die "Couldn't cp $srcFile to $destFile: err: $!";
            }

            # make sure time is right, in case file; file might have been newer,
            # speculation that cp -p silently failed if user doesn't own destDir
            @stat = stat($destFile) or die "Couldn't stat '$destFile'; err: $!";
            $mtime = $stat[9];

            my $softLink = $file;
            $softLink =~ s/\.$suffix$/-$mtime.$suffix/;

            # Delete obsolete symlinks
            for my $f (@destFiles) {
                if($f =~ /^$prefix-\d+\.$suffix$/ || $f =~ /^$prefix-v\d+\.$suffix$/) {
                    if($f ne $softLink) {
                        print STDERR "Deleting old soft-link $f\n" if($debug);
                        unlink($f) || die "Couldn't unlink obsolete softlink '$f'; err: $!";
                    }
                }
            }
            # create new symlink
            if(!(-l "$softLink")) {
                my $cmd = "ln -s $file $softLink";
                print STDERR "cmd: $cmd\n" if($debug);
                !system($cmd) || die "Couldn't $cmd; err: $!";
            }
        } else {
            # On production machines, we make a copy of git file with the CGI version burned in (see redmine #5104).
            # We also copy over a non-versioned copy for use by static files.
            my $versionedFile = $file;
            $versionedFile =~ s/\.$suffix$/-v$cgiVersion.$suffix/;
            for my $destFile ($file, $versionedFile) {
                my $update = 0;
                my $exists = 0;
                if(-e $destFile) {
                    my @destStat = stat("$destFile") or die "Couldn't stat '$destFile'; err: $!";
                    $update = ($destStat[9] < $mtime);
                    $exists = 1;
                } else {
                    $update = 1;
                }
                $update = $update || $force;
                if($update) {
                    # delete obsolete files
                    for my $f (@destFiles) {
                        if(-e $f && $f =~ /^$prefix-v\d+\.$suffix$/) {
                            if($f ne $destFile) {
                                print STDERR "Deleting old version of file $file: '$f'\n" if($debug);
                                unlink($f) || die "Couldn't unlink obsolete versioned file '$f'; err: $!";
                            }
                        }
                    }
                    if($exists) {
                        unlink($destFile) || die "Couldn't unlink '$destFile'";
                    }
                    if($minify && $suffix eq 'js') {
                        my $cmd = "/usr/bin/java -jar $minifyJar $srcFile -o $destFile";
                        print STDERR "cmd: $cmd\n" if($debug);
                        !system($cmd) || die "Couldn't run cmd '$cmd': err: $!";
                    } else {
                        my $cmd = "cp -p $srcFile $destFile";
                        print STDERR "cmd: $cmd\n" if($debug);
                        !system($cmd) || die "Couldn't $cmd; err: $!";
                    }
                }
            }
        }
    }
}
