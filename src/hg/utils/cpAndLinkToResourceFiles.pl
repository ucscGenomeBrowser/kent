#!/usr/bin/env perl

# Copy files to resource directory; make versioned soft-links as appropriate (and delete obsolete ones too).

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
my $minifyJar = "/cluster/home/larrym/tmp/yuicompressor-2.4.6/build/yuicompressor-2.4.6.jar";   # not sure where to install the jar (in utils dir?)

GetOptions("exclude=s" => \$exclude, "destDir=s" => \$destDir, "debug" => \$debug, "force" => \$force, "minify" => \$minify,
           "versionFile=s" => \$versionFile,  "forceVersionNumbers" =>  \$forceVersionNumbers);

if($exclude) {
    %exclude = map { $_ => 1} split(/\s*,\s*/, $exclude);
}

usage("Missing/invalid destDir '$destDir'") if(!$destDir || !(-d $destDir));

my $host = $ENV{HOST};
if(!defined($host)) {
    $host = `/bin/hostname`;
    chomp($host);
}

# Use version based links in production sites, mtime based links in dev sites (see redmine #3170)
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
        my @stat = stat("$cwd/$file") or die "Couldn't stat '$file'; err: $!";
        my $mtime = $stat[9];
        my ($prefix, $suffix);
        if($file =~ /(.+)\.([a-z]+)$/) {
            $prefix = $1;
            $suffix = $2;
        }
        # update destination file as appropriate
        my $update = 0;
        my $destFile = $file;
        if(-e $destFile) {
            my @destStat = stat("$destFile") or die "Couldn't stat '$destFile'; err: $!";
            $update = ($destStat[9] < $mtime);
        } else {
            $update = 1;
        }
        $update = $update || $force;
        if($update) {
            my $done;
            if (-e $destFile) {
                unlink($destFile) || die "Couldn't unlink $destFile'; err: $!";
            }
            if($debug) {
                print STDERR "cp -p $cwd/$file $destFile\n";
            }
            if($minify && $suffix eq 'js') {
                my $cmd = "/usr/bin/java -jar $minifyJar $cwd/$file -o $destFile";
                print STDERR "cmd: $cmd\n" if($debug);
                !system($cmd) || die "Couldn't run cmd '$cmd': err: $!";
                my @destStat = stat($destFile);
                if($destStat[7] >= $stat[7]) {
                    # Don't bother to minify if minified file is larger than original (this happens when file has already been minified).
                    print STDERR "Skipping minify for $cwd/$file because it is apparently already minified" if($debug);
                } else {
                    $done = 1;
                }
            }
            if(!$done) {
                !system("cp -p $cwd/$file $destFile") || die "Couldn't cp $cwd/file to $destFile: err: $!";
            }
        }

        if($prefix && $suffix) {
            # make sure time is right, in case file; file might have been newer,
            # speculation that cp -p silently failed if user doesn't own destDir
            @stat = stat($destFile) or die "Couldn't stat '$destFile'; err: $!";
            $mtime = $stat[9];

            my $softLink = $file;
            if($useMtimes) {
                $softLink =~ s/\.${suffix}$/-$mtime.${suffix}/;
            } else {
                $softLink =~ s/\.${suffix}$/-v$cgiVersion.${suffix}/;
            }
            # Delete obsolete symlinks
            for my $f (@destFiles) {
                if(($useMtimes && $f =~ /^$prefix-\d+\.$suffix$/) || ($useMtimes && $f =~ /^$prefix-v\d+\.$suffix$/)) {
                    if($f ne $softLink) {
                        print STDERR "Deleting old soft-link $f\n" if($debug);
                        unlink($f) || die "Couldn't unlink obsolete softlink '$softLink'; err: $!";
                    }
                }
            }
            # create new symlink
            if(!(-l "$softLink")) {
                print STDERR "ln -s $file $softLink\n" if($debug);
                !system("ln -s $file $softLink") || die "Couldn't ln -s $file; err: $!";
            }
        }
    }
}
