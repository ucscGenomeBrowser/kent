#!/usr/bin/perl -w
#
# Simple test for TrackDb module.  Make sure we're able to 
# get the list of trackNames for each database of interest and 
# for TrackDb's default database.
#

# Figure out path of executable so we can add perllib to the path.
use FindBin qw($Bin);
use lib "$Bin/perllib";
use TrackDb;

my @dbs = ('genome-centdb', 'hg7', 'hg10', undef);

foreach my $db (@dbs) {
    my $dbName = $db;
    $dbName = 'default' if (! defined $db);
    print "\nConnecting to database $dbName ...\n";

    # Connect to default database:
    my $tdb = new TrackDb($db);

    # Get the list of trackNames and print each one:
    my @trackNames = $tdb->getTrackNames();
    print "trackNames:\n";
    print "-----------\n";
    foreach my $t (@trackNames) {
	print "$t\n";
    }
    print "[end of $dbName trackNames]\n";

    # Clean up.
    $tdb->DESTROY();
} # end each db

