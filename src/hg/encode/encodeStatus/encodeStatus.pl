#!/usr/bin/env perl
# encodeStatus - show or change status of an ENCODE data submission
#
# Requires that the new status be the correct one to follow the
# existing status.  Statuses that can be changed manually do not
# overlap statuses set by the pipeline automation.
#
# TODO
#
# When status changes to "approved", create a pushQ entry, using the tables listed in load.ra

# $Id: encodeStatus.pl,v 1.2 2008/07/17 18:36:56 larrym Exp $

use warnings;
use strict;

use Getopt::Long;
use lib "/cluster/bin/scripts";
use HgDb;

# Last status set by pipeline
my $LOADED_STATUS = "loaded";

# statuses after $LOADED_STATUS are set by DCC staff currently, and must be used in this order
my @statuses = ($LOADED_STATUS, "displayed", "approved", "reviewing", "released");

my $instance = 'prod';
my $force;

GetOptions("instance=s" => \$instance, "force" => \$force);
if (@ARGV < 1 || @ARGV > 2) {
    usage();
}

my $pipelineDb = "encpipeline_$instance";
my $project = $ARGV[0];
my $newStatus = $ARGV[1];
my $isName = $project !~ /^\d+$/;

my $db = HgDb->new(DB => $pipelineDb);

if (!defined($newStatus)) {
    my $sth;
    if ($isName) {
        $sth = $db->execute("SELECT status, id FROM projects WHERE name=?", $project);
    } else {
        $sth = $db->execute("SELECT status, name FROM projects WHERE id=?", $project);
    }
    my $projectInfo;
    if(my @row = $sth->fetchrow_array()) {
        $projectInfo = $row[0];
    }
    if (!defined($projectInfo)) {
        usage("ERROR: Project '$project' not found\n");
    }
    print "$projectInfo\n";
    exit 0;
}

# Change project status in database
my $sth;
if ($isName) {
    $sth = $db->execute("SELECT status FROM projects WHERE name=?", $project);
} else {
    $sth = $db->execute("SELECT status FROM projects WHERE id=?", $project);
}
my $oldStatus;
if(my @row = $sth->fetchrow_array()) {
    $oldStatus = $row[0];
}
if (!defined($oldStatus)) {
    usage("ERROR: Project '$project' not found in instance '$instance'\n");
}

for my $i (1 .. @statuses - 1) {
    my $s = $statuses[$i];
    if ($s eq $newStatus) {
        if ($oldStatus ne $statuses[$i - 1] && !$force) {
            usage("ERROR: New status '$newStatus' cannot follow '$oldStatus'\n");
        }
        my $sth;
        if ($isName) {
            $db->execute("UPDATE projects SET status=? WHERE name=?", $newStatus, $project);
        } else {
            $db->execute("UPDATE projects SET status=? WHERE id=?", $newStatus, $project);
        }
        print "Project '$project' successfully updated from '$oldStatus' to '$newStatus'\n";
        exit 0;
    }
}

usage("Status '$newStatus' is invalid\n");

sub usage
{
    my ($msg) = @_;
    if($msg) {
        print STDERR "$msg\n";
    }
    my $valid = join(", ", @statuses);
    print STDERR <<END;
usage: encodeStatus [-instance=instanceName] [-force] project-id|project-name [status]

valid statuses: $valid

-instance	Default instance is 'prod'
-force		Use if you want to set a status that is not normally allowed (e.g. to reset
		to an earlier status).
END
    exit(1);
}
