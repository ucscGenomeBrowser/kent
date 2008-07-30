#
# RAFile: methods to access and parse ra files
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/RAFile.pm instead.
#
# $Id: RAFile.pm,v 1.2 2008/07/29 18:48:06 larrym Exp $

package RAFile;

use warnings;
use strict;

use Carp;
use vars qw(@ISA @EXPORT_OK);
use Exporter;

@ISA = qw(Exporter);

@EXPORT_OK = (
    qw( readRaFile
      ),
);

sub readRaFile
{
# Read records from a .ra file into a hash of hashes and return it.
# $keyField is the used as the primary key in the ra file.
# The primary key values are used as the hash key values in the returned hash.
# e.g.:
# 	('Bernstein' => {wrangler => 'tom'}, 'Crafword' => {wranger => 'dick'})
    my ($file, $keyField) = @_;
    open(RA, $file) || die "ERROR: Can't open RA file \'$file\'\n";
    my %ra = ();
    my $raKey = undef;
    foreach my $line (<RA>) {
        chomp $line;
        $line =~ s/^\s+//;
        $line =~ s/\s+$//;
        if (!length($line)) {
            # blank line indicates end of record
            $raKey = undef;
            next;
        }
        next if $line =~ /^#/;
        if ($line =~ m/^$keyField\s+(.*)/) {
            $raKey = $1;
            $ra{$raKey}->{$keyField} = $raKey;
        } else {
            die "ERROR: Missing key '$keyField' before line '$line'\n" if(!defined($raKey));
            my ($key, $val) = split('\s+', $line, 2);
            $ra{$raKey}->{$key} = $val;
        }
    }
    close(RA);
    return %ra;
}

1;
