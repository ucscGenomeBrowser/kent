#!/bin/env perl

use strict;
use warnings;
use File::stat;
use Getopt::Long;
use Math::BigInt;

sub usage() {
    print STDERR <<_END_;
usage: lostTables.pl -age=N [-wigVarFile=pathName.wigVar]
required argument:
-age=N - specify N hours to age out tables, must be exactly 72

optional argument:
-wigVarFile=pathName.wigVar - to output a wiggle variable
   - step file with date vs. size information.
_END_
    exit 255;
}

# for a graph offset, this date will be X=0, resolution 1 second per base
#	2010-01-01 12:00:00 1262376000
#	from mktime command in kent source src/utils/timing/
my $timeZero = 1262376000;
my $wigVarFile = '';
my %wigData;
my $help = '';

my $ageHours = -1;

my $optResult = GetOptions("age=i" => \$ageHours,
	"help", => \$help,
	"-wigVarFile=s", => \$wigVarFile);

if ($ageHours == -1 || $help) {
    usage;
}

if ($optResult != 1) {
    printf STDERR "ERROR: incorrect option given to script.";
    usage;
}

if ($ageHours != 72) {
    printf STDERR "ERROR: given age: %d\n", $ageHours;
    printf STDERR "ERROR: age must be 72 hours\n";
    usage;
}
my $ageSeconds = $ageHours * 60 * 60;

if ($wigVarFile) {
    printf STDERR "#\toutput graph data to wiggle file: $wigVarFile\n";
    open (WV, ">$wigVarFile") or die "can not write to $wigVarFile";
    printf WV "track type=wiggle_0 name=customTrashSize\n";
    printf WV "variableStep chrom=chr1\n";
}
printf STDERR "#\tage: %d hours, %d seconds\n", $ageHours, $ageSeconds;

if (! defined($ENV{'HGDB_CONF'})) {
    $ENV{'HGDB_CONF'} = "/data/home/qateam/.ct.hg.conf";
}

my %showTables;	# list of table names from MySQL show tables
my %metaTables; # list of table names from metaInfo
my %lostTables; # list of tables from MySQL but not in metaInfo
my $tableCount = 0;
my $metaTableCount = 0;
my $lostCount = 0;

###########################################################################
## read in table list from metaInfo name column
###########################################################################

my $hgsql = "/cluster/bin/x86_64/hgsql";

open (FH, "$hgsql -N -e 'select name from metaInfo;' customTrash |") or
    die "can not run hgsql 'select name from metaInfo' command";
while (my $table = <FH>) {
    chomp $table;
    if (exists($metaTables{"$table"})) {
	printf STDERR "WARNING: metaInfo duplicate table name ? %s\n", $table;
    }
    ++$metaTableCount;
    $metaTables{"$table"} = 1;
}
close (FH);

printf STDERR "# metaInfo table count: %d\n", $metaTableCount;

###########################################################################
## read in table list from "show tables" MySQL operation
##	ignore table names: history, extFile, metaInfo
###########################################################################

open (FH, "$hgsql -N -e 'show tables;' customTrash | egrep -v 'history|extFile|metaInfo' |") or
    die "can not run hgsql 'show tables' command";

while (my $table = <FH>) {
    chomp $table;
    if (exists($showTables{"$table"})) {
	printf STDERR "WARNING: duplicate table name ? %s\n", $table;
    }
    $showTables{"$table"} = 1;
    ++$tableCount;
    if (!exists($metaTables{"$table"})) {
	$lostTables{"$table"} = 1;
	++$lostCount;
    }
}
close (FH);

printf STDERR "# customTrash table count: %d, lost table count: %d\n",
    $tableCount, $lostCount;

###########################################################################
## finally, scan MySQL files to determine date and size information
###########################################################################

my $custTrashDir = "/data/mysql/customTrash";
chdir $custTrashDir;
printf "working in: '%s'\n", $custTrashDir;
my $fileCount = 0;
my $agedOut = 0;
my $notAgedOut = 0;
my $totalBytes = Math::BigInt->bzero();
my $agedBytes = Math::BigInt->bzero();
my $notAgedBytes = Math::BigInt->bzero();
open (FH, "find . -type f | grep '.frm\$' | sed -e 's#./##'|") or die "can not run find in $custTrashDir";
my $nowTimeStamp = `date "+%s"`;
chomp $nowTimeStamp;
printf "#\ttimestamp now: %d\n", $nowTimeStamp;
printf "# table name\tage of:\t\tmtime\tctime\t\tatime\tsizes: fyi MYI MYD\n";
while (my $table = <FH>) {
    ++$fileCount;
    chomp $table;
    $table =~ s/.frm//;
    next if ($table =~ m/metaInfo/);
    next if ($table =~ m/extFile/);
    next if ($table =~ m/history/);
    my $frm = stat("$table.frm");
    my $myi = stat("$table.MYI");
    my $myd = stat("$table.MYD");
    my $sizeFrm = Math::BigInt->new($frm->size);
    my $sizeMyi = Math::BigInt->new($myi->size);
    my $sizeMyd = Math::BigInt->new($myd->size);
    $totalBytes->badd($sizeFrm);
    $totalBytes->badd($sizeMyi);
    $totalBytes->badd($sizeMyd);
    if ($wigVarFile) {
	my $start = $frm->mtime - $timeZero;
	if ($start > 0) {
	    my $sizePt;
	    if (exists($wigData{$start})) {
		$sizePt = $wigData{$start};
	    } else {
		my $size = Math::BigInt->bzero();
		$sizePt = \$size;
		$wigData{$start} = $sizePt;
	    }
	    $$sizePt->badd($sizeFrm);
	    $$sizePt->badd($sizeMyi);
	    $$sizePt->badd($sizeMyd);
	}
    }
    next if (!exists($lostTables{"$table"}));  # checking for lost tables
    my $tableAgeSeconds = $nowTimeStamp - $frm->mtime;  # what is their age
    if ($tableAgeSeconds > $ageSeconds) {
	++$agedOut;
	$agedBytes->badd($sizeFrm);
	$agedBytes->badd($sizeMyi);
	$agedBytes->badd($sizeMyd);
	printf "%s\t%12d%12d%12d\t%d %d %d\n", $table, $nowTimeStamp-$frm->mtime, $nowTimeStamp-$frm->ctime, $nowTimeStamp-$frm->atime, $frm->size, $myi->size, $myd->size;
    } else {
	++$notAgedOut;
	$notAgedBytes->badd($sizeFrm);
	$notAgedBytes->badd($sizeMyi);
	$notAgedBytes->badd($sizeMyd);
	printf "# %s\t%12d%12d%12d\t%d %d %d\n", $table, $frm->mtime, $frm->ctime, $frm->atime, $frm->size, $myi->size, $myd->size;
    }
}
printf "#\ttable count: $fileCount, total bytes in all tables: %s\n#\tlost tables aged out: $agedOut, bytes aged: %s, not aged out: $notAgedOut, bytes: %s\n",
	$totalBytes->bstr(), $agedBytes->bstr(), $notAgedBytes->bstr();

# if requested, output wiggle file for time vs. size of data created
#	at that second.
if ($wigVarFile) {
    foreach my $start (sort { $a <=> $b} keys %wigData) {
	my $sizePt = $wigData{$start};
	printf WV "%d\t%s\n", $start, $$sizePt->bstr();
    }
    close (WV);
}
