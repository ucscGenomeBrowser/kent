#!/usr/bin/env perl
# loadCosmic.pl - load bi-monthly COSMIC data via URL provided by Sanger
# Data is created in a per version directory on the hive and then loaded into mysql
# Sanity checks are written to STDERR

use strict;

use Cwd;
use Getopt::Long;
use lib "/cluster/bin/scripts";
use HgDb;

my ($verbose, $oldVer, $dryRun);
GetOptions("verbose" => \$verbose, "oldVer=i" => \$oldVer, "dryRun" => \$dryRun);

my $assembly = shift(@ARGV) || die "Missing assembly argument";
my $srcUrl = shift(@ARGV) || die "Missing source URL argument";
my ($fileName, $ver, $cmd, $gzipped);
my $cosmicBed =  "cosmic.bed";
my $db = HgDb->new(DB => $assembly);

if($srcUrl =~ m,/([^/]+?_v(\d+)_.+\.csv)$,) {
    $fileName = $1;
    $ver = $2;
} elsif ($srcUrl =~ m,/([^/]+?_v(\d+)_.+\.csv\.gz)$,) {
    $fileName = $1;
    $ver = $2;
    $gzipped = 1;
} elsif ($srcUrl =~ m,/([^/]+?_v(\d+)\.csv\.gz)$,) {
    $fileName = $1;
    $ver = $2;
    $gzipped = 1;
} else {
    die "Missing version number in srcUrl";
}


if($dryRun) {
    print STDERR "Loading COSMIC v$ver (dryRun)\n";
} else {
    print STDERR "Loading COSMIC v$ver\n";
}

if(!$oldVer) {
    $oldVer = $ver - 1;
}

my $outsideDir = "/hive/data/outside/cosmic/v$ver";
if(!(-d $outsideDir)) {
    mkdir($outsideDir) || die "mkdir($outsideDir) failed; err: $!";
}

if(!(-e "$outsideDir/$fileName")) {
    # save raw data file, received by email, in $outsideDir (skip fetch if we already did it).
    $cmd = "wget -O $outsideDir/$fileName $srcUrl";
    !system($cmd) || die "cmd '$cmd' failed: err: $!";
}

my $loadDir = "/hive/data/genomes/$assembly/bed/cosmic/v$ver";
my $oldLoadDir = "/hive/data/genomes/$assembly/bed/cosmic/v$oldVer";
my $oldBed = "$oldLoadDir/$cosmicBed";
if(!(-d $loadDir)) {
    mkdir($loadDir) || die "mkdir($loadDir) failed; err: $!";
}
if(!(-d $oldLoadDir)) {
    die "oldDir '$oldLoadDir' doesn't exist";
}
if(!(-e $oldBed)) {
    die "oldBed '$oldLoadDir' doesn't exist";
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

# source file has some crap in it (header and empty lines at the end) which has to be removed before loading into cosmicRaw
my $tabFile = "EnsMutExp_v$ver.tab";
$cmd = "egrep '^COSMIC' $fileName | sed -e 's/\t//g' | sed -e 's/,/\t/g' > $tabFile";
!system($cmd) || die "cmd '$cmd' failed: err: $!";

# pull bed4 out of cosmicRaw data
my %ids;
my $lineNumber = 0;
my  ($oldLen, $newLen);
open(FILE, $tabFile) || die "Can't open '$tabFile'; err: $!";
open(BED, ">$cosmicBed") || die "Can't create '$cosmicBed'; err: $!\n";
while(<FILE>) {
    chomp;
    my @fields = split(/\t/);
    # Fields in csv file:
    # Source, COSMIC_MUTATION_ID, gene name, accession_number,mut_description,mut_syntax_cds, mut_syntax_aa,chromosome,GRCh37 start,GRCh37 stop,mut_nt, mut_aa, tumour_site, mutated_samples, examined_samples, mut_freq%
    # use grch37_start-1 for our zero based chromStart and convert their chr23 and chr24 to chrX and chrY.
    $lineNumber++;
    if(!exists($ids{$fields[1]})) {
        $ids{$fields[1]}++;
        $fields[8]--;
        if($fields[7] eq '23') {
            $fields[7] = 'chrX';
        } elsif ($fields[7] eq '24') {
            $fields[7] = 'chrY';
        } elsif ($fields[7] eq '25') {
            $fields[7] = 'chrM';
        } elsif ($fields[7] >= 1 && $fields[7] <= 22) {
            $fields[7] = 'chr' . $fields[7];
        } else {
            die "Invalid chr '$fields[7]' on $lineNumber of $fileName";
        }
        print BED join("\t", $fields[7], $fields[8], $fields[9], $fields[1]), "\n";
        $newLen++;
    }
}
close(FILE);
close(BED);

# Generate some stats for sanity checking

my %oldIds;
my $deletedIds = 0;
my $addedIds = 0;

open(FILE, $oldBed) || die "Can't open '$oldBed'; err: $!";
while(<FILE>) {
    chomp;
    my @fields = split(/\t/);
    $oldIds{$fields[3]}++;
    $oldLen++;
    if(!exists($ids{$fields[3]})) {
        # Ids are deleted (e.g. COSM142830 was deleted in v58), presumably b/c of very low frequency, but this is rare.
        print STDERR "Deleted id: $fields[3]\n" if($verbose);
        $deletedIds++;
    }
}
close(FILE);

for my $id (keys %ids) {
    if(!exists($oldIds{$id})) {
        $addedIds++;
    }
}

$cmd = "bedIntersect -aHitAny $oldBed $cosmicBed stdout | wc -l";
my $out = `$cmd`;
if($out =~ /(\d+)/) {
     print STDERR sprintf("New length: $newLen\nOld length: $oldLen\nPercent bed overlap with previous version: %.2f%%\nNumber of deleted IDs: $deletedIds\nNumber of added IDs: $addedIds\n", 100 * ($1 / $oldLen));
} else {
    die "cmd '$cmd' didn't work as expected";
}

if(!$dryRun) {
    $cmd = "hgsql $assembly -e 'drop table cosmicRaw'";
    !system($cmd) || die "cmd '$cmd' failed: err: $!";
    $cmd = "hgsql $assembly < ~/kent/src/hg/lib/cosmicRaw.sql";
    !system($cmd) || die "cmd '$cmd' failed: err: $!";

    $cmd = "hgLoadSqlTab $assembly cosmicRaw ~/kent/src/hg/lib/cosmicRaw.sql $tabFile";
    !system($cmd) || die "cmd '$cmd' failed: err: $!";

    $cmd = "hgLoadBed -allowStartEqualEnd  $assembly cosmic $cosmicBed";
    !system($cmd) || die "cmd '$cmd' failed: err: $!";

    # add a new row to trackVersion
    $db->execute("insert into hgFixed.trackVersion (db, name, who, version, updateTime, comment, source) values ('$assembly', 'cosmic', '$ENV{USER}', '$ver', now(), 'loaded by loadCosmic.pl', '$srcUrl')");
}

exit(0);
