#
# TrackDb: interface to the trackDb database.
#
# Currently, all this does is return a list of trackName values.
# Note the hardcoded default database, username & password.
#
package TrackDb;

use strict;
use vars qw($VERSION @ISA @EXPORT_OK);
use Exporter;

use FindBin qw($Bin);
use lib $Bin;
use HgConf;

use DBI;
use Carp;

@ISA = qw(Exporter);
@EXPORT_OK = qw( new DESTROY getTrackNames );
$VERSION = '0.01';

#
# Some parameters...
#
my $defaultDb = 'hg10';

#
# new: create a TrackDb object.
# Mandatory argument: <none>
# Optional arguments: $dbName (name of database to use instead of default)
#
sub new {
    my $class = shift;
    my $dbName = shift;
    confess "Too many arguments" if (defined shift);
    $dbName = $defaultDb if (! defined $dbName);
    my $hgConf = HgConf->new($main::opt_hgConf);
    my $host = $hgConf->lookup('db.host');
    if ($host && $host ne 'localhost') {
        $host = ";host=$host";
    } else {
        $host = "";
    }
    my $username = $hgConf->lookup('db.user');
    my $password = $hgConf->lookup('db.password');
    my $dbh = DBI->connect("DBI:mysql:database=$dbName$host", $username, $password);
    confess "Can't connect to mysql database $dbName!" if (! $dbh);
    my $this = {
	'dbh' => $dbh,
    };
    bless $this, $class;
} # end new


#
# DESTROY: perl is supposed to call this destructor when the last 
# ref to an object goes away.
#
sub DESTROY {
    my $this = shift;
    confess "Too many arguments" if (defined shift);
    $this->{'dbh'}->disconnect();
} # end DESTROY

#
# getTrackNames
# Mandatory argument: <none>
#
sub getTrackNames {
    my $this = shift;
    confess "Too many arguments" if (defined shift);
    my $results = 
	$this->{'dbh'}->selectcol_arrayref("SELECT tableName from trackDb;");
    return @{$results};
} # end getTrackNames


#
# getTrackNamesTypes
# Mandatory argument: <none>
#
sub getTrackNamesTypes {
    my $this = shift;
    confess "Too many arguments" if (defined shift);
    my $results = 
	$this->{'dbh'}->selectall_arrayref("SELECT tableName,type from trackDb;");
    my %namesTypes = ();
    foreach my $row (@{$results}) {
      my ($name,$type) = @{$row};
      $namesTypes{$name} = $type;
    }
    return %namesTypes;
} # end getTrackNames


# perl packages need to end by returning a positive value:
1;
