#
# TrackDb: interface to the trackDb database.
#
package TrackDb;

use strict;
use vars qw($VERSION @ISA @EXPORT_OK);
use Exporter;

use DBI;
use Carp;

@ISA = qw(Exporter);
@EXPORT_OK = qw( new DESTROY getTrackNames );
$VERSION = '0.01';

#
# Some parameters...
#
my $defaultDb = 'hg10';
my $username  = 'hguser';
my $password  = 'hguserstuff';

#
# new
#
sub new {
    my $class = shift;
    my $dbName = shift;
    confess "Too many arguments" if (defined shift);
    $dbName = $defaultDb if (! defined $dbName);
    my $dbh = DBI->connect("DBI:mysql:$dbName", $username, $password);
    confess "Can't connect to mysql database $dbName!" if (! $dbh);
    my $this = {
	'dbh' => $dbh,
    };
    bless $this, $class;
} # end new


#
# DESTROY -- perl is supposed to call this destructor when the last 
# ref goes away.
#
sub DESTROY {
    my $this = shift;
    confess "Too many arguments" if (defined shift);
    $this->{'dbh'}->disconnect();
} # end DESTROY

#
# getTrackNames
#
sub getTrackNames {
    my $this = shift;
    confess "Too many arguments" if (defined shift);
    my $results = 
	$this->{'dbh'}->selectcol_arrayref("SELECT tableName from trackDb;");
    return @{$results};
} # end getTrackNames


# perl packages need to end by returning a positive value:
1;
