#!/usr/bin/env perl

# doEncodeReport.pl - munge trackDb and pipeline projects table to report
#                       submitted experiments.
# Reporting format:
#       Project, Lab, Data Type, Cell Type, Experiment Parameters, Freeze, Submission Date, Release Date, Status, Version, Submission Ids

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit the CVS'ed source at:
# $Header: /projects/compbio/cvsroot/kent/src/hg/encode/encodeValidate/doEncodeReport.pl,v 1.4 2009/11/19 19:23:17 kate Exp $

# TODO: warn if variable not found in cv.ra

use warnings;
use strict;

use File::stat;
use File::Basename;
use Getopt::Long;
use English;
use Carp qw(cluck);
use Cwd;
use IO::File;
use File::Basename;

#use lib "/cluster/bin/scripts";
use lib "/cluster/home/kate/kent/src/hg/utils/automation";
use Encode;
use HgAutomate;
use HgDb;
use RAFile;
use SafePipe;

use vars qw/
    $opt_configDir
    $opt_verbose
    /;

# Global variables
our $pipeline = "encpipeline_prod";
our $pipelinePath = "/hive/groups/encode/dcc/pipeline/" . $pipeline;
#our $configPath = $pipelinePath . "/config";
our $configPath = "/cluster/home/kate/kent/src/hg/encode/encodeValidate" . "/config";
our $assembly = "hg18";

sub usage {
    print STDERR <<END;
usage: doEncodeReport.pl

options:
    -configDir=dir      Path of configuration directory, containing
                        metadata .ra files (default: submission-dir/../config)
    -verbose=num        Set verbose level to num (default 1).
END
exit 1;
}

############################################################################
# Main

my $wd = cwd();

my $ok = GetOptions("configDir=s",
                    "verbose=i",
                    );
usage() if (!$ok);
$opt_verbose = 1 if (!defined $opt_verbose);

# Determine submission, configuration, and output directory paths
HgAutomate::verbose(4, "Using submission directory tree at\'$pipelinePath\'\n");

if (defined $opt_configDir) {
    if ($opt_configDir =~ /^\//) {
        $configPath = $opt_configDir;
    } else {
        $configPath = "$wd/$opt_configDir";
    }
}

if(!(-d $configPath)) {
    die "configPath '$configPath' is invalid; Can't find the config directory\n";
}
HgAutomate::verbose(4, "Config directory path: \'$configPath\'\n");

my %terms = Encode::getControlledVocab($configPath);
# use pi.ra file to map pi/lab/institution/grant/project
my $labRef = Encode::getLabs($configPath);
my %labs = %{$labRef};

# parse all trackDb entries w/ metadata setting into hash of submissions
#   key is lab+dataType+cell+vars+version
#   key is lab+dataType+cell+vars

my $dbh = HgDb->new(DB => $assembly);
my $sth = $dbh->execute(
        "select settings, tableName from trackDb where settings like \'\%metadata\%\'");
my @row;
my %experiments = ();
printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", 
        "Project", "Lab", "Data_Type", "Cell_Type", "Experiment_Parameters",
        "Version", "Freeze", "Submit_Date", "Release_Date", 
        "Status", "Submission_IDs");
while (@row = $sth->fetchrow_array()) {
    my $settings = $row[0];
    my $tableName = $row[1];
    # strip down to just tag=value in the metadata line
    $settings =~ tr/\n/|/;
    $settings =~ s/^.*metadata //;  
    $settings =~ s/\|.*$//;
    my $ref = Encode::metadataLineToHash($settings);
    my %metadata =  %{$ref};
    my %experiment = ();
    $experiment{"project"} = defined($metadata{"grant"}) ? $metadata{"grant"} : "unknown";
    foreach my $lab (keys %labs) {
        if ($labs{$lab}->{"grant"} eq $metadata{"grant"}) {
            $experiment{"project"} = $labs{$lab}->{"project"};
            last;
        }
    }
    $experiment{"lab"} = $metadata{"lab"};
    # strip off PI name in parens
    $experiment{"lab"} =~ s/\(\w+\)//;
    $experiment{"dataType"} = lc($metadata{"dataType"});

    $experiment{"cell"} = "none";
    if (defined($metadata{"cell"})) {
        $experiment{"cell"} = $metadata{"cell"};
    }
    #print STDERR "    tableName  " . $tableName . "\n";
    print STDERR "    tableName  " . $tableName . "\n";

    # determine term type for experiment parameters 
    my %vars = ();
    my @termTypes = (keys %terms);
    foreach my $var (keys %metadata) {
        foreach my $termType (@termTypes) {
            next if ($termType eq "Cell Line");
            if (defined($terms{$termType}{$metadata{$var}})) {
                $vars{$termType} = $metadata{$var};
            }
        }
    }
    # experimental params -- iterate through all tags, lookup in cv.ra, 
     $experiment{"vars"} = "none";
    if (scalar(keys %vars) > 0) {
        # alphasort vars by term type, to assure consistency
        # TODO: define explicit ordering for term type in cv.ra
        $experiment{"vars"} = "";
        foreach my $termType (sort keys %vars) {
            $experiment{"vars"} = $experiment{"vars"} .  $vars{$termType} . ";";
        }
        chop $experiment{"vars"};
    }
    $experiment{"version"} = "V1";
    if (defined($metadata{"submittedDataVersion"})) {
        $experiment{"version"} = $metadata{"submittedDataVersion"};
        $experiment{"version"} =~ s/^.*(V\d+).*/$1/;
    }
    $experiment{"freeze"} = $metadata{"dataVersion"};
    $experiment{"freeze"} =~ s/^ENCODE (...).*20(\d\d).*$/$1-$2/;

    $experiment{"submitDate"} = "00-00-00";
    if (defined($metadata{"dateSubmitted"})) {
        $experiment{"submitDate"} = $metadata{"dateSubmitted"};
    }
    $experiment{"releaseDate"} = "unknown";

    # note that this was found in trackDb (not just in pipeline, below)
    $experiment{"trackDb"} = 1;

    # get release date from 'lastUpdated' field of submission dir
    $experiment{"status"} = "unknown";

    # a few tracks are missing the subId
    my $subId = (defined($metadata{"subId"})) ? $metadata{"subId"} : 0; 

    #print STDERR "    ID  " . $subId "\n";
    #die "undefined project" unless defined($experiment{"project"});
    #die "undefined lab" unless defined($experiment{"lab"});
    #die "undefined dataType" unless defined($experiment{"dataType"});
    #die "undefined cell" unless defined($experiment{"cell"});
    #die "undefined vars" unless defined($experiment{"vars"});
    #die "undefined freeze" unless defined($experiment{"freeze"});
    #die "undefined freeze" unless defined($experiment{"freeze"});
    #die "undefined submitDate" unless defined($experiment{"submitDate"});
    #die "undefined releaseDate" unless defined($experiment{"releaseDate"});
    #die "undefined status" unless defined($experiment{"status"});

    # create key for experiment: lab+dataType+cell+vars+version
    # create key for experiment: lab+dataType+cell+vars
    my $expKey = $experiment{"lab"} . "+" . $experiment{"dataType"} . "+" . 
                                $experiment{"cell"} . "+" . 
                                $experiment{"vars"};
                                #$experiment{"vars"} . "+" .  $experiment{"version"};

    print STDERR "KEY: " . $expKey . "\n";

    # save in a hash of experiments, 
    #   keyed by lab+dataType+cell+vars
    # (may need to add version to key)
    # subId -- lookup, add to list if exists
    if (defined($experiments{$expKey})) {
        my %ids = %{$experiments{$expKey}->{"ids"}};
        print STDERR "CHECKING: " . $expKey . " new ID: " . $subId . " IDs: " . join(",", keys %ids) . "\n";
        if (!defined($experiment{$expKey}->{"ids"}->{$subId})) {
            $experiments{$expKey}->{"ids"}->{$subId} = $subId;
            %ids = %{$experiments{$expKey}->{"ids"}};
            print STDERR "MERGED: " . $expKey . " IDs: " . join(",", keys %ids) . "\n";
        }
    } else {
        my %ids = ();
        $ids{$subId} = $subId;
        $experiment{"ids"} = \%ids;
        $experiments{$expKey} = \%experiment;
        print STDERR "ADDING: " . $expKey . " ID: " . $subId . "\n";
    }

}
$sth->finish;
$dbh->{DBH}->disconnect;

# merge in submissions from pipeline projects table
# use status in projects table to assign statuses to submissions already found in trackDb

$dbh = HgDb->new(DB => $pipeline);
$sth = $dbh->execute( 
    "select lab, data_type, metadata, created_at, updated_at, status, id, name from projects order by id");

while (@row = $sth->fetchrow_array()) {
    my $lab = $row[0];
    my $status = $row[5];
    my $id = $row[6];
    my $name = $row[7];
    # lookup in lab from hash to find project
    my $project = "unknown";
    if (defined($lab) && defined($labs{$lab})) {
        $project = $labs{$lab}->{"project"};
    } else {
        warn "lab undefined, subId=" . $id . " " . $status . " " . $name . "\n";
        next;
    }
    if ($project eq "unknown") {
        warn "     project unknown for lab " . $lab . " subId=" . $id . " " . $name . "\n";
    }
    my $dataType = $row[1];
# note: lcase the datatype before generating key (inconsistent in DAFs)
    my $created_at = $row[3];
    my $updated_at = $row[4];
    my $version = "V1";         # need this in project table
        my $cell = "none";
    my $varString = "none";

# get cell type and experimental vars
    if (defined($row[2])) {
        my @metadata = split("; ", $row[2]);
        foreach my $expVars (@metadata) {
# one experiment
            my @vars = split(", ", $expVars);

            my $termType;
# extract cell type, order other vars by term type alpha order
# determine term type for experiment parameters 
            my %varHash = ();
            my @termTypes = (keys %terms);
            foreach my $var (@vars) {
                foreach $termType (@termTypes) {
                    if (defined($terms{$termType}{$var})) {
                        if ($termType eq "Cell Line") {
                            $cell = $var;
                        } else {
                            $varHash{$termType} = $var;
                        }

                    }
                }
            }
            # experimental params -- iterate through all tags, lookup in cv.ra, 
            if (scalar(keys %varHash) > 0) {
                # alphasort vars by term type, to assure consistency
                $varString = "";
                foreach $termType (sort keys %varHash) {
                    $varString = $varString . $varHash{$termType} . ";";
                }
                chop $varString;
            }
            my $expKey = $lab . "+" . lc($dataType) .  "+" .
                                $cell . "+" .  $varString;
                                #$cell . "+" .  $varString . "+" . $version;
            print STDERR "KEY+STATUS+ID: " . $expKey . "|" . $status . "|" . $id . "\n";
            if (defined($experiments{$expKey})) {
                my %experiment = %{$experiments{$expKey}};
                if (defined($experiment{"trackDb"})) {
                    if (defined $experiment{"ids"}->{$id}) {
                        # this id is in trackDb
                        if (!defined $experiment{"statusId"} || $id gt $experiment{"statusId"}) {
                            # need to change status
                            $experiment{"statusId"} = $id;
                            if ($experiment{"status"} ne $status && 
                                        $experiment{"status"} ne "unknown") {
                                warn("STATUS change for KEY: " . $expKey . 
                                        " OLD: " . $experiment{"status"} . 
                                                " from ID: " . $experiment{"statusId"} . 
                                        " NEW: " . $status . " from ID " . $id . "\n");
                            }
                            $experiment{"status"} = $status;
                            print STDERR "    Setting status: " . $expKey . 
                                " from ID: " . $id . " to " . $status . "\n";
                        }
                    }
                } else {
                    # not in trackDb but already added to hash, update tempStatus if id is bigger
                    if ($id gt $experiment{"tempStatusId"}) {
                        # need to change status
                        if ($experiment{"tempStatus"} ne $status) {
                            warn("Temp STATUS change for non-trackDb KEY: " . $expKey . 
                                    " OLD: " . $experiment{"tempStatus"} . 
                                            " from ID: " . $experiment{"tempStatusId"} . 
                                    " NEW: " . $status . " from ID " . $id . "\n");
                        }
                        $experiment{"tempStatus"} = $status;
                        $experiment{"tempStatusId"} = $id;
                        print STDERR "    Setting temp status of KEY: " . $expKey . 
                                " from ID: " . $id . " to " . $status . "\n";
                    }
                }
                # save updated experiment to hash
                $experiments{$expKey} = \%experiment;
            } else {
                # not in trackDb or found yet in projects table
                my %experiment = ();
                $experiment{"project"} = $project;
                $experiment{"lab"} = $lab;
                $experiment{"dataType"} = lc $dataType;
                $experiment{"cell"} = $cell;
                $experiment{"version"} = $version;
                my %ids = ();
                $ids{$id} = $id;
                $experiment{"ids"} = \%ids;
                $experiment{"tempStatus"} = $status;
                $experiment{"tempStatusId"} = $id;
                $experiment{"vars"} = $varString;
                $experiment{"submitDate"} = $created_at;
                # trim off time
                $experiment{"submitDate"} =~ s/ \d\d:\d\d:\d\d//;
                $experiment{"releaseDate"} = 
                        ($status eq "released" ? $updated_at : "none");
                $experiment{"releaseDate"} =~ s/ \d\d:\d\d:\d\d//;
                $experiment{"freeze"} = "unknown";
                $experiments{$expKey} = \%experiment;
                print STDERR "ADDING2: " . $expKey . " IDs: " . 
                        join(",", keys %ids) . " PROJ: " . $project . "\n";
            }
        }
    }
}
$sth->finish;
$dbh->{DBH}->disconnect;

# print out results
foreach my $key (keys %experiments) {
    my %experiment = %{$experiments{$key}};
    my %ids = %{$experiment{"ids"}};
    print STDERR "    IDs: " . join(",", keys %ids) . " KEY:  " . $key . "\n";
    die "undefined project" unless defined($experiment{"project"});
    die "undefined lab" unless defined($experiment{"lab"});
    die "undefined dataType" unless defined($experiment{"dataType"});
    die "undefined cell" unless defined($experiment{"cell"});
    die "undefined vars" unless defined($experiment{"vars"});
    die "undefined freeze" unless defined($experiment{"freeze"});
    die "undefined freeze" unless defined($experiment{"freeze"});
    die "undefined submitDate" unless defined($experiment{"submitDate"});
    die "undefined releaseDate" unless defined($experiment{"releaseDate"});
    if (!defined $experiment{"status"}) {
        $experiment{"status"} = $experiment{"tempStatus"};
    }
    die "undefined status" unless defined($experiment{"status"});
    printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", 
                $experiment{"project"}, $experiment{"lab"}, 
                $experiment{"dataType"}, $experiment{"cell"}, 
                $experiment{"vars"}, $experiment{"version"}, 
                $experiment{"freeze"}, $experiment{"submitDate"}, 
                $experiment{"releaseDate"}, $experiment{"status"}, 
                join(",", keys %ids));
}

exit 0;
