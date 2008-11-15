#!/usr/bin/env perl
# encodeStatus - show or change status of an ENCODE data submission
#
# Requires that the new status be the correct one to follow the
# existing status.  Statuses that can be changed manually do not
# overlap statuses set by the pipeline automation.
#
# $Id: encodeStatus.pl,v 1.4 2008/11/15 01:14:01 kate Exp $

use warnings;
use strict;

use Getopt::Long;
use lib "/cluster/bin/scripts";
use HgDb;
use Encode;

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

my $pipelineDb = Encode::pipelineDb($instance);
my $project = $ARGV[0];
my $newStatus = $ARGV[1];
my $isName = $project !~ /^\d+$/;

my $db = HgDb->new(DB => $pipelineDb);
my $sth;
if ($isName) {
    $sth = $db->execute("SELECT status, id FROM projects WHERE name = ?", $project);
} else {
    $sth = $db->execute("SELECT status, id FROM projects WHERE id = ?", $project);
}
my ($oldStatus, $id);
if(my @row = $sth->fetchrow_array()) {
    $oldStatus = $row[0];
    $id = $row[1];
}

if (!defined($oldStatus)) {
    usage("ERROR: Project '$project' not found in instance '$instance'\n");
}

my $dir = Encode::projectDir($instance, $id);
if(!(-d $dir)) {
    die "Can't find project dir '$dir'";
}

if (!defined($newStatus)) {
    print "$oldStatus\n";
    exit 0;
}

for my $i (0 .. @statuses - 1) {
    my $s = $statuses[$i];
    if ($s eq $newStatus) {
        if ($newStatus ne $LOADED_STATUS && ($oldStatus ne $statuses[$i - 1] && !$force)) {
            usage("ERROR: New status '$newStatus' cannot follow '$oldStatus'\n");
        }
        $db->execute("UPDATE projects SET status = ? WHERE id = ?", $newStatus, $id);
        $db->execute("INSERT INTO project_status_logs (project_id, status, created_at, who) \
                        VALUES (?, ?, NOW(), ?)", $id, $newStatus, getlogin());
        print "Project '$project' successfully updated from '$oldStatus' to '$newStatus'\n";
        if($newStatus eq 'approved') {
            my $pushQFile = "$dir/out/$Encode::pushQFile";
            if(-e $pushQFile) {
                # We probably s/d change the code to just run this once we enter production phase (if $instance eq 'prod')
                print <<END;
You must execute this command to add the pushQ entry:

cat $pushQFile | ssh -x hgwbeta hgsql -N qapushq
END
            } else {
                die "Can't find pushQFile '$pushQFile'";
            }
        }
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
