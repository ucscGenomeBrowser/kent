#
# HgDb: Class interface to the mysql databases
# Get's relevant mysql connect data from the .hg.conf file
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/HgDb.pm instead.

# $Id: HgDb.pm,v 1.5 2008/08/29 22:16:10 larrym Exp $

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
# $args{USER}, $args{PASSWORD} and $args{HOST} are optional (and override .hg.conf values)
    my ($class, %args) = (@_);
    my $ref = {};
    if(!defined($args{DB})) {
        die "Missing \$args{DB}";
    }
    my $dsn = "DBI:mysql:$args{DB}";
    my $confFile = "$ENV{HOME}/.hg.conf";
    if(! -e $confFile) {
        die "Cannot locate conf file: '$confFile'";
    }
    open(CONF, $confFile);
    for (<CONF>) {
        next if /^#/;
        if(/^include/) {
            # XXXX TODO: support "include ../cgi-bin/hg.conf"
            die "include ... syntax not currently supported";
        }
        for my $name (qw(host user password)) {
            if(/^db\.$name\s*=\s*(.+)/) {
                $ref->{uc($name)} = $1;
            }
        }
    }
    close(CONF);
    for (keys %args) {
        # %args override values in conf file.
        $ref->{$_} = $args{$_};
    }
    if($ref->{HOST}) {
        $dsn .= ";host=$ref->{HOST}";
    }
    $ref->{DBH} = DBI->connect($dsn, $ref->{USER}, $ref->{PASSWORD}) or die "Couldn't connect to db: $ref->{DB}";
    bless $ref, 'HgDb';
    return $ref;
}

sub execute
{
# Execute given query with @params substituted for placeholders in the query
# Returns $sth
    my ($db, $query, @params) = (@_);
    my $sth = $db->{DBH}->prepare($query) or die "prepare for query '$query' failed; error: " . $db->{DBH}->errstr;
    if(!$sth->execute(@params)) {
        die "execute for query '$query' failed; error: " . $db->{DBH}->errstr;
    }
    return $sth;
}

sub quickQuery
{
# Execute given query with @params substituted for placeholders in the query.
# Returns first field in first row.
# Return undef if query comes up empty.
    my ($db, $query, @params) = (@_);
    my $sth = $db->{DBH}->prepare($query) or die "prepare for query '$query' failed; error: " . $db->{DBH}->errstr;
    if(!$sth->execute(@params)) {
        die "execute for query '$query' failed; error: " . $db->{DBH}->errstr;
    }
    my @row = $sth->fetchrow_array();
    return @row ? $row[0] : undef;
}

sub tableExist
{
    my ($db, $tableName) = @_;
    my $retval = 0;
    my $sth = $db->execute("show tables like ?", $tableName);
    my @row = $sth->fetchrow_array();
    if(@row && $row[0]) {
        $retval = 1 ;
    }
    return $retval;
}

sub dropTable
{
    my ($db, $tableName) = @_;
    $db->execute("drop table $tableName") || die "Couldn't drop table '$tableName'";
}

sub dropTableIfExist
{
    my ($db, $tableName) = @_;
    if(tableExist($db, $tableName)) {
        dropTable($db, $tableName);
    }
}

1;
