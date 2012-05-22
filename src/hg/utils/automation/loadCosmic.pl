#!/usr/bin/env perl
# loadCosmic.pl - load bi-monthly COSMIC data via URL provided by them

use strict;

use Cwd;

my $db = shift(@ARGV) || die "Missing assembly argument";
my $srcUrl = shift(@ARGV) || die "Missing source URL argument";
my ($fileName, $ver, $cmd, $gzipped);

if($srcUrl =~ m,/([^/]+?_v(\d+)_.+\.csv)$,) {
    $fileName = $1;
    $ver = $2;
} elsif ($srcUrl =~ m,/([^/]+?_v(\d+)_.+\.csv\.gz)$,) {
    $fileName = $1;
    $ver = $2;
    $gzipped = 1;
} else {
    die "Missing version number in file argument";
}

my $outsideDir = "/hive/data/outside/cosmic/v$ver";
if(!(-d $outsideDir)) {
    mkdir($outsideDir) || die "mkdir($outsideDir) failed; err: $!";
}

# save raw data file, received by email, in $outsideDir

$cmd = "wget -O $outsideDir/$fileName $srcUrl";
!system($cmd) || die "cmd '$cmd' failed: err: $!";

my $loadDir = "/hive/data/genomes/$db/bed/cosmic/v$ver";
if(!(-d $loadDir)) {
    mkdir($loadDir) || die "mkdir($loadDir) failed; err: $!";
}
if($gzipped) {
    $fileName =~ s/.gz$//;
    $cmd = "zcat $outsideDir/$fileName.gz > $loadDir/$fileName";
} else {
    $cmd = "cp $outsideDir/$fileName $loadDir/$fileName";
}
!system($cmd) || die "cmd '$cmd' failed: err: $!";

my $cwd = getcwd();
chdir($loadDir) || die "Couldn't chdir into '$loadDir'; err: $!";

my $tabFile = "EnsMutExp_v$ver.tab";

# source file has some crap in it (header and empty lines at the end) which has to be removed
$cmd = "egrep '^COSMIC' $fileName | sed -e 's/\t//g' | sed -e 's/,/\t/g' > $tabFile";
!system($cmd) || die "cmd '$cmd' failed: err: $!";

$cmd = "hgsql $db -e 'drop table cosmicRaw'";
!system($cmd) || die "cmd '$cmd' failed: err: $!";
$cmd = "hgsql $db < ~/kent/src/hg/lib/cosmicRaw.sql";
!system($cmd) || die "cmd '$cmd' failed: err: $!";

$cmd = "hgLoadSqlTab $db cosmicRaw ~/kent/src/hg/lib/cosmicRaw.sql $tabFile";
!system($cmd) || die "cmd '$cmd' failed: err: $!";

# use  grch37_start-1 for our zero based chromStart and convert their chr23 and chr24 to chrX and chrY.

$cmd = "hgsql $db -N -e 'select \"chr\", chromosome, grch37_start-1, grch37_stop, cosmic_mutation_id from cosmicRaw' | grep -v NULL | sed -e 's/chr\t/chr/' | sort -u|sed -e 's/chr23/chrX/' | sed -e 's/chr24/chrY/' > cosmic.bed";
!system($cmd) || die "cmd '$cmd' failed: err: $!";

$cmd = "hgLoadBed -allowStartEqualEnd  $db cosmic cosmic.bed";
!system($cmd) || die "cmd '$cmd' failed: err: $!";

exit(0);
