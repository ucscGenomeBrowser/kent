#!/usr/local/bin/perl

use strict;

use Net::hostent;
use Socket;
use Getopt::Long;
use lib "/cluster/bin/scripts";
use HgDb;

sub usage
{
    print STDERR <<END;
Usage: geoMirrorNode.pl ips.txt

Uses the "central" profile from .hg.conf.

Maps a list of hosts/IPs to their geoMirror node (input file can contain either IPs or host names)
prints out three columns:

host IP node

host != IP when the input file specifies an IP instead of a host (i.e. we lookup the IP when a host name is specified).
END
    exit(1);
}

my $help;
my $table = 'geoIpNode';
my $profile = 'central';
GetOptions("help" => \$help);

if($help) {
    usage();
}

my $db = HgDb->new(PROFILE => $profile);

for my $name (<>) {
    if($name =~ /^#/) {
       print $name;
       next;
   }
    chomp($name);
    my $ip;
    if($name =~ /^\d+\.\d+\.\d+\.\d+$/) {
        $ip = $name;
    } else {
        my $h = gethost($name);
        if ($h) {
            $ip = inet_ntoa($h->addr);
        } else {
            print STDERR "Couldn't find IP for host '$name'\n";
            next;
        }
    }
    my $numeric = unpack("N", inet_aton($ip));
    my $node = $db->quickQuery("select node from $table where $numeric >= ipStart and $numeric <= ipEnd order by ipStart desc limit 1");
    if(!$node) {
        $node = 'unmapped';
    }
    print join("\t", $name, $ip, $node), "\n";
}
