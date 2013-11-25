#!/usr/bin/env perl

use strict;
use warnings;
use Time::HiRes qw( usleep ualarm gettimeofday tv_interval nanosleep
		      clock_gettime clock_getres clock_nanosleep clock
                      stat );

my $argc = scalar(@ARGV);

if ($argc < 1) {
    printf STDERR "usage: fileStatsFromFind.pl <trashFile.list>\n";
    printf STDERR "used by the script: trashSizeMonitor.sh\n";
    printf STDERR "to calculate statistics about files in the /trash/ directory \n";
    exit 255
}

my %topDirs;	# key is directory name, value is file count under directory
my %depthCount;	# key is directory name, value is total depth count
my $totalFiles = 0;
my $topLevelFiles = 0;

my ($seconds0, $microseconds0) = gettimeofday();
my $t0 = $seconds0 + ($microseconds0/1000000.0);

# expecting file names to explicitly include "/trash/"
while (my $file = shift) {
    open (FH, "/bin/sed -e s'#.*/trash/##' $file|") or die "can not read $file";
    while (my $line = <FH>) {
        chomp $line;
        $line =~ s#^\./##;
        if ($line =~ m#/#) {
	    my @pathSpec = split('/', $line);
            $topDirs{$pathSpec[0]} += 1;
            if (exists($depthCount{$pathSpec[0]})) {
              $depthCount{$pathSpec[0]} = scalar(@pathSpec)
                 if (scalar(@pathSpec) > $depthCount{$pathSpec[0]});
            } else {
              $depthCount{$pathSpec[0]} = scalar(@pathSpec);
            }
        } else {
            ++$topLevelFiles;
        }
        $totalFiles++;
    }
    close (FH);
}
my ($seconds1, $microseconds1) = gettimeofday();
my $t1 = $seconds1 + ($microseconds1/1000000.0);

my $duList = "";
my $dirCount = 0;
foreach my $key (sort keys %topDirs) {
     ++$dirCount;
     $duList .= " $key";
}

printf "directory count: %d\n", $dirCount;
printf "top level file count: %d\n", $topLevelFiles;
printf "lower directory file count: %d\n", $totalFiles-$topLevelFiles;
printf "total file count: %d\n", $totalFiles;
