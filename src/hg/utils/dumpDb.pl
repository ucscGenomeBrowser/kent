#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit ~/kent/src/hg/utils/dumpDb.pl instead.

# $Id: dumpDb.pl,v 1.1 2006/05/04 16:13:05 hiram Exp $

use strict;
use warnings;

sub usage()
{
print "dumpDb.pl - dump database tables and maintain date/time stamps\n";
print "usage: dumpDb.pl <db>\n";
print "must be run in an empty directory.\n";
print "the date/time stamps of the dumped files will be the Update_time\n";
print "  of the database table.\n";
print "the hgFindSpec_* and trackDb_* tables will not be saved.\n";
print "expects to use the command hgsql and hgsqldump to access the db.\n";
}

my $argc = scalar(@ARGV);

if (1 != $argc ) { usage; exit 255; }

my $dirEmpty=`ls`;
chomp $dirEmpty;
if (length($dirEmpty) != 0)
    {
    print "ERROR: this directory is not empty\n";
    usage;
    exit 255;
    }
my $db = shift;
my %tableTimes;	#	key is table name, value is Update_time

open (PH,"hgsql -N -e 'show table status;' $db|") or
	die "can not run show table status command"; 
while (my $line = <PH>)
{
    chomp $line;
    my @fields = split('\t', $line);
    my $numFields = scalar(@fields);
    #	field 0 is the table name, 11 is the Update_time in MySQL 4
    #	field 0 is the table name, 12 is the Update_time in MySQL 5 when != NULL
    my $YMD = "0";
    my $HMS = "0";
    #	sometimes there isn't any date in the status ?
    if ($numFields > 14)
	{
	if ($fields[12] =~ /NULL/)
	    {
	    if ($fields[11] !~ /NULL/)
		{
		($YMD, $HMS) = split('\s',$fields[11]);
		}
	    } else {
	    ($YMD, $HMS) = split('\s',$fields[12]);
	    }
	} else {
	    ($YMD, $HMS) = split('\s',$fields[11]);
	}
    if (length($YMD) > 2)
	{
	my ($YYYY, $MM, $DD) = split('-',$YMD);
	my ($hr, $min, $sec) = split(':',$HMS);
	#	create touch -t format YYYYMMDDhhmm.ss
	$tableTimes{$fields[0]} =
	    sprintf("%4d%02d%02d%02d%02d.%02d", $YYYY, $MM, $DD, $hr, $min, $sec);
	}
}

close (PH);

print "hgsqldump --all -c --tab=. $db\n";
print `hgsqldump --all -c --tab=. $db`, "\n";

foreach my $table (keys %tableTimes)
{
    if ($table =~ /^trackDb_.*|^hgFindSpec_.*/)
	{
	`rm -f $table.sql $table.txt`;
	} else {
	if ( ! -r "$table.txt" )
	    {
	    print "ERROR: can not find file $table.txt\n";
	    exit 255;
	    }
	#	the .txt files are owned by mysql, change ownership
	print `mv $table.txt $table.txt.tmp;cp $table.txt.tmp $table.txt;rm -f $table.txt.tmp`;
	`touch -t $tableTimes{$table} $table.sql $table.txt`;
	}
}
