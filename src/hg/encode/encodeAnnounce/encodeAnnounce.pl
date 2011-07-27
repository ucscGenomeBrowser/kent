#!/usr/bin/env perl

# encodeAnnounce.pl - use pushQ to generate a template announcement
#
# usage:  encodeAnnounce.pl <assembly> <date>
#
#       Creates an announcement template for all tracks released on the
#       selected assembly since the selected date

use warnings;
use strict;


use lib "/cluster/bin/scripts";
use HgDb;
use HTML::Entities;

# Global variables
our $assembly;  # required command-line arg
our $date;      # required command-line arg

our $pushqDb = "qapushq";
our $pushqHost = "mysqlbeta";
our $betaHost = "mysqlbeta";
our $publicHost = "genome.ucsc.edu";
our $afterwordFile = "announceBoilerplate.html";

sub usage {
    print STDERR <<END;
usage: encodeAnnounce.pl <assembly> <YYYY-MM-DD> 
END
exit 1;
}

############################################################################
# Main

usage() if (scalar(@ARGV) != 2);
($assembly, $date) = @ARGV;

# open pushQ to extract track labels and URL
my $dbhPushq = HgDb->new(DB => $pushqDb, HOST => $pushqHost);
my $dbhBetaMeta = HgDb->new(DB => $assembly, HOST => $betaHost);

my $sth = $dbhPushq->execute( 
    "select track, releaseLog, qadate, releaseLogUrl from pushQ where dbs=\'$assembly\' and releaseLog like '%ENCODE%' and priority='L' and qadate > '$date' order by qadate");

my @row;
my @tracks;
my @releases;
while (@row = $sth->fetchrow_array()) {
    my ($track, $longLabel, $qadate, $url) = @row;
    push @tracks, $track;
    $url =~ s|^.*cgi-bin|http://$publicHost/cgi-bin/|;
    my %release = ("track" => $track, "longLabel" => $longLabel, 
                   "date" => $qadate, "url" => $url);
    push @releases, \%release;
}
$sth->finish;

# print subject header
my $org = ($assembly eq "mm9" ? "Mouse" : "");
print "\nSubject: $org ENCODE data releases: " . join(", ", @tracks) . "\n\n";

print "The ENCODE Data Coordination Center at UCSC is pleased to announce the release of the following new data tracks on the $assembly genome browser:\n";

print "\n";

# print info for each released track
foreach my $release (@releases) {

    # print label
    my $label = $release->{'longLabel'};
    print "$label ($release->{'date'})\n";
    $label =~ s/./-/g;
    print "$label\n";

    # print 1st paragraph of description
    open (HTML, "wget -q -O - '$release->{url}' |");
    while (<HTML>) {
        last if /[Hh]2>\s*Description/;
    }
    while (my $line = <HTML>) {
         last if $line =~ /<\/[Pp]>/;
         #$line =~ 's/<[^>]*>//g';
         chomp $line;
         print(decode_entities($line) . ' ');
    }
    # print URL
    print "\n\n$release->{'url'}\n\n\n";
    close HTML;
}

system("cat", "$afterwordFile");

exit 0;
