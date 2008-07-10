#
# HgDb: Class interface to the mysql databases
# Get's relevant mysql connect data from the hg.conf file
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/HgDb.pm instead.

package HgDb;

use warnings;
use strict;

use Carp;
use DBI;

use vars qw(@ISA @EXPORT_OK);
use Exporter;

@ISA = qw(Exporter);

sub new
{
# $args{DB} is required
# $args{USER}, $args{PASSWORD} and $args{HOST} are optional (and override hg.conf if present)
    my ($class, %args) = (@_);
    my $ref = {};
    my $db = $args{DB} or die "Missing \$args{DB}";
    my $dsn = "DBI:mysql:$args{DB}";
    my $confFile = "/usr/local/apache/cgi-bin-$ENV{USER}/hg.conf";
    if(! -e $confFile) {
        $confFile = "/usr/local/apache/cgi-bin/hg.conf";
    }
    open(CONF, $confFile);
    my $conf = join("", <CONF>);
    close(CONF);
    my ($host, $user, $password);
    if($conf =~ /db\.host=(.*)/) {
        $host = $1;
    }
    if($args{HOST}) {
        $host = $args{HOST};
    }
    if($conf =~ /db\.user=(.*)/) {
        $user = $1;
    }
    if($args{USER}) {
        $host = $args{USER};
    }
    if($conf =~ /db\.password=(.*)/) {
        $password = $1;
    }
    if($args{PASSWORD}) {
        $host = $args{PASSWORD};
    }
    my $dbh = DBI->connect($dsn, $user, $password) or die "Couldn't connect to db: $db";
    $ref->{DBH} = $dbh;
    bless $ref, 'HgDb';
    return $ref;
}

sub execute
{
# Execute given query with @params substituted for placeholders in the query
# Returns $sth
    my ($db, $query, @params) = (@_);
    my $sth = $db->{DBH}->prepare($query) or die "prepare for query '$query' failed; error: " . $db->{DBH}->errstr;
    my $rv = $sth->execute(@params) or die "execute for query '$query' failed; error: " . $db->{DBH}->errstr;
    return $sth;
}
