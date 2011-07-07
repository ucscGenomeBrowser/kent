#!/usr/bin/env perl

# encodeReport.pl - use metaDb tables to generate report of submitted and released experiments
#
# Reporting formats:
# ALL: Project, Project-PI, Lab, Lab-PI, Data_Type, Cell_Type, Experiment_Parameters, Experimental_Factors, Freeze, Submit_Date, Release_Date, Status, Submission_IDs, Accession, Assembly, Strain, Age, Treatment, Exp_ID, DCC_Accession, Lab_IDs

# DCC:       Project(institution), Lab(institution), Data Type, Cell Type, Experiment Parameters, Freeze, Submit_Date, Release_Date, Status, Assembly, Submission Ids, Accession, Exp_ID, DCC_Accession, Lab_IDs
# Briefly was:
# DCC:       Project(institution), Lab(institution), Data Type, Cell Type, Experiment Parameters, Freeze, Submit_Date, Release_Date, Status, Submission Ids, Accession, Assembly

# NHGRI:     Project(PI), Lab (PI), Assay (TBD), Data Type, Experimental Factor, Organism (human/mouse), Cell Line, Strain, Tissue (TBD), Stage/Age, Treatment, Date Data Submitted, Release Date, Status, Submission Id, GEO/SRA IDs, Assembly, Exp_ID, DCC_Accession, Lab_IDs

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit the git source at:
# kent/src/hg/encode/encodeReport/encodeReport.pl

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

use lib "/cluster/bin/scripts";
use Encode;
use HgAutomate;
use HgDb;
use RAFile;
use SafePipe;

use vars qw/
    $opt_configDir
    $opt_expTable
    $opt_limit
    $opt_verbose
    /;

# Global variables
our $pipeline = "encpipeline_prod";
our $pipelinePath = "/hive/groups/encode/dcc/pipeline/" . $pipeline;
our $configPath = $pipelinePath . "/config";
our $assembly;  # required command-line arg
our $expTable = "encodeExp";

sub usage {
    print STDERR <<END;
usage: encodeReport.pl <assembly>

options:
    -configDir=dir      Path of configuration directory, containing
                        cv.ra file (default: pipeline config dir)
    -expTable=table     Alternate experiment table to use for fishing experiment ID's
    -limit=string       Limit to objects matching /string/ (for debug)
    -verbose=num        Set verbose level to num (default 1).
END
exit 1;
}

############################################################################
# Main

my $wd = cwd();

my $ok = GetOptions("configDir=s",
                    "expTable=s",
                    "limit=s",
                    "verbose=i",
                    );
usage() if (!$ok);
usage() if (scalar(@ARGV) != 1);
($assembly) = @ARGV;
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

if (defined $opt_expTable) {
    $expTable = $opt_expTable;
}

if (!(-d $configPath)) {
    die "configPath '$configPath' is invalid; Can't find the config directory\n";
}
HgAutomate::verbose(4, "Config directory path: \'$configPath\'\n");

my %terms = Encode::getControlledVocab($configPath);
my %tags = Encode::getControlledVocabTags($configPath);
my @termTypes = (keys %terms);

my %labs = %{$terms{"lab"}};
my %grants = %{$terms{"grant"}};
my $dataType;
my %dataTypes = %{$terms{"dataType"}};
my $var;
my $termType;
my $subId;

printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", 
        "Project", "Project-PI", "Lab", "Lab-PI", "Data_Type", "Cell_Type", 
        "Experiment_Parameters", "Experimental_Factors", 
        #"Version", 
        "Freeze", "Submit_Date", "Release_Date", 
        "Status", "Submission_IDs", "Accession", "Assembly", 
        "Strain", "Age", "Treatment", "Exp_ID", "DCC_Accession", "Lab_IDs");

# get experiment-defining variables for each composite from the metaDb
open(MDB, "mdbPrint $assembly -table=metaDb -all -line | grep 'objType=composite' |") 
                or die "Can't run mdbPrint\n";
my %metaLines = ();
while (my $line = <MDB>) {
    chomp $line;
    $line =~ s/^.*metadata //;  
    (my $objName, my $settings) = split(" ", $line, 2);

# DEBUG
    if (defined($opt_limit)) {
        next unless $objName =~ /$opt_limit/;
    }

    $metaLines{$objName} = $settings;
    print STDERR "     Composite OBJNAME: $objName   SETTINGS: $settings\n";
}
close MDB;
my %composites = ();
foreach my $objName (keys %metaLines) {
    my $settings = $metaLines{$objName};
    my $ref = Encode::metadataLineToHash($settings);
    my %metadata =  %{$ref};
    if (defined($metadata{"expVars"})) {
        $composites{$objName} = $metadata{"expVars"};
    }
}

# get metadata objects from metaDb and create hash of experiments
open(MDB, "mdbPrint $assembly -table=metaDb -all -line |") or die "Can't run mdbPrint\n";
while (my $line = <MDB>) {
    chomp $line;
    $line =~ s/^.*metadata //;  
    (my $objName, my $settings) = split(" ", $line, 2);

# DEBUG
    if (defined($opt_limit)) {
        next unless $objName =~ /$opt_limit/;
    }

    $metaLines{$objName} = $settings;
    print STDERR "     OBJNAME: $objName   SETTINGS: $settings\n";
}
close MDB;

# Create experiments hash
my %experiments = ();

foreach my $objName (keys %metaLines) {
    my $settings = $metaLines{$objName};
    print STDERR "     SETTINGS: $settings\n";
    my $ref = Encode::metadataLineToHash($settings);
    my %metadata =  %{$ref};

    # short-circuit if this didn't come in from pipeline
    # NOTE: this also excludes composite objects
    next unless defined($metadata{"subId"});

    my %experiment = ();
    $experiment{"project"} = "unknown";
    $experiment{"projectPi"} = "unknown";
    $experiment{"labPi"} = "unknown";
    $experiment{"objName"} = $objName;

    my $lab = "unknown";
    if (defined($metadata{"lab"})) {
        $lab = $metadata{"lab"};
        # TODO: STILL NEEDED ?
        $lab =~ s/\(\w+\)//;
        # TODO: Fix 'lab=' metadata for UT-A to be consistent
        # TODO: STILL NEEDED ?
        $lab = "UT-A" if ($lab eq "UT-Austin");
    }
    foreach $var (keys %labs) {
        #my $strippedVar = $var;
        #$strippedVar =~ s/\(\w+\)//;
        if (lc($var) eq lc($lab)) {
            $experiment{"lab"} = $lab;
            $experiment{"labPi"} = $labs{$var}->{"labPi"};
            my $grantPi = $labs{$var}->{"grantPi"};
            $experiment{"projectPi"} = $grantPi;
            $experiment{"project"} = $grants{$grantPi}->{"projectName"};
            warn "lab/project mismatch in settings $settings" 
                    unless ($metadata{"grant"} eq $experiment{"projectPi"});
        }
    }
    if ($experiment{"project"} eq "unknown") {
        warn "lab $lab not found, from settings $settings";
    }
    # for now, force all Yale projects to be Yale lab (until metadata in projects table can match)
    #if ($metadata{"grant"} eq "Snyder") {
        #$experiment{"lab"} = "Yale";
    #}

    # look up dataType case-insensitive, as tag or term, save as tag, term and label
    # TODO:  Still needed ?
    my $dType = $metadata{"dataType"};
    $dataType = $dType;
    my $dataTypeTerm;
    foreach $var (keys %dataTypes) {
        if (lc($dType) eq lc($var) || lc($dType) eq lc($dataTypes{$var}->{"tag"})) {
            $dataType = $dataTypes{$var}->{"tag"};
            $dataTypeTerm = $dataTypes{$var}->{"term"};
        }
    }
    $experiment{"dataType"} = $dataType;
    $experiment{"dataTypeTerm"} = $dataTypeTerm;

    $experiment{"cell"} = "none";
    if (defined($metadata{"cell"})) {
        $experiment{"cell"} = $metadata{"cell"};
    }
    print STDERR "    objName  " . $objName . "\n";

    # determine term type for experiment parameters - extracted from composite expVars
    my $composite = $metadata{"composite"};
    my $expVars;
    if (defined($composite)) {
        $expVars = $composites{$composite};
        if (!defined($expVars)) {
            warn "composite $composite has no expVars, skipping";
            next;
        }
    } else  {
        $expVars = $metadata{"expVars"}
    }

    my @varTermTypes = split(",", $expVars);

    my %vars = ();
    foreach $var (keys %metadata) {
        # special handling for 'treatment=None' and 'age=immortalized' which
        # are defaults
        next if ($var eq "treatment" && $metadata{$var} eq "None");
        next if ($var eq "age" && $metadata{$var} eq "immortalized");
        # consider adding:
        # next if ($var eq "strain" && $metadata{$var} eq "C57BL/6");
        foreach $termType (@varTermTypes) {
            # exclude the 'standard' experiment-defining vars
            next if (($termType eq "cell") or ($termType eq "lab") or ($termType eq "dataType"));
            if (lc($termType) eq lc($var)) {
                $vars{$termType} = $metadata{$var};
            }
        }
    }

    my @varList = keys %vars;
    #print STDERR "VARS: " . "@varList" . "\n";
    # treat version like a variable
    # TODO: How to handle now?
    if (defined($metadata{"submittedDataVersion"})) {
        my $version = $metadata{"submittedDataVersion"};
        # strip comment
        $version =~ s/^.*(V\d+).*/$1/;
        $vars{"version"} = $version;
    }

    # collapse multiple terms for GEO accession
    # TODO:  Still needed ?
    if (defined($metadata{"accession"})) {
        $experiment{"accession"} = $metadata{"accession"};
    } elsif (defined($metadata{"geoSampleAccession"})) {
        $experiment{"accession"} = $metadata{"geoSampleAccession"};
    } elsif (defined($metadata{"geoSeriesAccession"})) {
        $experiment{"accession"} = $metadata{"geoSeriesAccession"};
    } else {
        $experiment{"accession"} = "";
    }

    # experiment ID's and accessions
    if (defined($metadata{"expId"})) {
        $experiment{"expId"} = $metadata{"expId"};
    } else {
        $experiment{"expId"} = 0;
    }
    if (defined($metadata{"dccAccession"})) {
        $experiment{"dccAccession"} = $metadata{"dccAccession"};
    } else {
        $experiment{"dccAccession"} = "";
    }

    # special columns in report -- collect even if not expermint-defining variables
    $experiment{"strain"} = defined($metadata{"strain"}) ? $metadata{"strain"} : "n/a";
    $experiment{"age"} = defined($metadata{"age"}) ? $metadata{"age"} : "n/a";
    $experiment{"treatment"} = (defined($metadata{"treatment"}) && lc($metadata{"treatment"}) ne "none") ? 
                                        $metadata{"treatment"} : "none";

    # experimental params -- iterate through all tags, lookup in cv.ra, 
     $experiment{"vars"} = "none";
     $experiment{"varLabels"} = "none";
     $experiment{"factorLabels"} = "none";
     if (scalar(keys %vars) > 0) {
        # alphasort vars by term type, to assure consistency
        # construct vars as list, semi-colon separated

        # string of expVars + version, for constructing keys
        $experiment{"vars"} = "";

        # abbreviated string of expVars + version for printing in report (DCC Experimental Vars column)
        $experiment{"varLabels"} = "";

        # string of expVars + version - (treatment, age, strain) for printing in report 
        #                       (NHGRI Experimental Factors column)
        $experiment{"factorLabels"} = "";

        foreach $termType (sort keys %vars) {
            $var = $vars{$termType};
            $experiment{"vars"} = $experiment{"vars"} .  $termType . "=" . $var . " ";
            my $varLabel = defined($terms{$termType}{$var}->{"label"}) ? 
                                $terms{$termType}{$var}->{"label"} : $var;

            # special handling of treatment, age, strain for NHGRI -- they have specific columns
            # so if they are exp vars, they are added only to varLabels column (unless they
            #    are defaults (treatment=None, age=immortalized or 8wks, strain=C56BL/6)
            if ($termType eq "treatment" && lc($varLabel) ne "none") {
                $experiment{"varLabels"} = $experiment{"varLabels"} .  $varLabel . ";";
            } elsif ($termType eq "age") {
                next if ($varLabel eq "immortalized");
                next if ($varLabel eq "adult-8wks");
                $experiment{"varLabels"} = $experiment{"varLabels"} .  $varLabel . ";";
            } elsif ($termType eq "strain" && lc($varLabel) ne "C57BL/6") {
                $experiment{"varLabels"} = $experiment{"varLabels"} .  $varLabel . ";";
            } else {
                # other experimental variables get added to both columns
                $experiment{"varLabels"} = $experiment{"varLabels"} .  $varLabel . ";";
                $experiment{"factorLabels"} = 
                        $experiment{"factorLabels"} .  ucfirst($termType) . "=" . $varLabel . " ";
            }
        }
        chop $experiment{"vars"};
        chop $experiment{"varLabels"};
        chop $experiment{"factorLabels"};
    }

    if (defined($metadata{"dataVersion"})) {
        $experiment{"freeze"} = $metadata{"dataVersion"};
        $experiment{"freeze"} =~ s/^ENCODE (...).*20(\d\d).*$/$1-20$2/;
        $experiment{"freeze"} =~ s/^post ENCODE (...).*20(\d\d).*$/post $1-20$2/;
    } else {
        $experiment{"freeze"} = "unknown";
    }

    $experiment{"submitDate"} = defined($metadata{"dateResubmitted"}) ?
                $metadata{"dateResubmitted"} : $metadata{"dateSubmitted"};

    $experiment{"releaseDate"} = "none";

    # a few tracks are missing the subId
    my $subId = (defined($metadata{"subId"})) ? $metadata{"subId"} : 0; 

    # create key for experiments (lab:%s dataType:%s cellType:%s vars:<var=val> <var2=val>)
    my $expKey = "lab:" . $lab .
                " dataType:" . $dataType .
                " cellType:" . $experiment{"cell"};
    if (defined $experiment{"vars"}) {
        $expKey = $expKey . 
                " vars:" .  $experiment{"vars"};
    }
    print STDERR "KEY: " . $expKey . "\n";

    # snag lab experiment ID's if any (may be a list)
    my @labIds;
    if (defined($metadata{"labExpId"})) {
        @labIds = split(',', $metadata{"labExpId"});
    }

    # save in a hash of experiments, 
    # (include version in vars)
    # subId -- lookup, add to list if exists
    if (defined($experiments{$expKey})) {
        # add subId
        # just for diagnostics
        my %ids = %{$experiments{$expKey}->{"ids"}};
        print STDERR "CHECKING: " . $expKey . " new ID: " . $subId . " IDs: " . join(",", keys %ids) . "\n";
        if (!defined($experiments{$expKey}->{"ids"}->{$subId})) {
            $experiments{$expKey}->{"ids"}->{$subId} = $subId;
            # just for diagnostics
            %ids = %{$experiments{$expKey}->{"ids"}};
            print STDERR "MERGED: " . $expKey . " IDs: " . join(",", keys %ids) . "\n";
        }
        if (defined($metadata{"labExpId"})) {
            # add labExpIds
            foreach my $labId (@labIds) {
                if (!defined($experiments{$expKey}->{"labIds"}->{$labId})) {
                    $experiments{$expKey}->{"labIds"}->{$labId} = $labId;
                }
            }
        }
    } else {
        # add subId
        my %ids = ();
        $ids{$subId} = $subId;
        $experiment{"ids"} = \%ids;

        # add labExpIds
        my %labIds = ();
        if (defined($metadata{"labExpId"})) {
            foreach my $labId (@labIds) {
                $labIds{$labId} = $labId;
            }
        }
        $experiment{"labIds"} = \%labIds;

        $experiments{$expKey} = \%experiment;
        print STDERR "ADDING: " . $expKey . " ID: " . $subId . "\n";
    }
}

# get submission status for all projects from the pipeline projects table
my %submissionStatus = ();

# TODO: determine how/if to keep/use the update time
my %submissionUpdated = ();
my $dbhPipeline = HgDb->new(DB => $pipeline);
my $sth = $dbhPipeline->execute( 
    "select id, name, status, updated_at from projects where db=\'$assembly\' order by id");

my @row;
while (@row = $sth->fetchrow_array()) {
    my $id = $row[0];
    my $name = $row[1];
    my $status = $row[2];
    my $updated_at = $row[3];
    $submissionStatus{$id} = $status;
    HgAutomate::verbose(2, "project: \'$name\' $id $status \n");
    $submissionUpdated{$id} = $updated_at;
    $submissionUpdated{$id} =~ s/ \d\d:\d\d:\d\d//;
}
$sth->finish;

# open pushQ to extract release dates
our $pushqDb = "qapushq";
our $pushqHost = "mysqlbeta";
our $betaHost = "mysqlbeta";
our $dbhPushq = HgDb->new(DB => $pushqDb, HOST => $pushqHost);
our $dbhBetaMeta = HgDb->new(DB => $assembly, HOST => $betaHost);

# fill in statuses and print out results
foreach my $key (keys %experiments) {
    my %experiment = %{$experiments{$key}};

    # get subIds for this experiment in this assembly
    my %ids = %{$experiment{"ids"}};
    my @ids = sort { $a <=> $b } keys %ids;
    print STDERR "    IDs: " . join(",", @ids) . " KEY:  " . $key . "\n";
    my %labIds = %{$experiment{"labIds"}};
    $experiment{"project"} = "unknown"  unless defined($experiment{"project"});
    $experiment{"projectPi"} = "unknown"  unless defined($experiment{"projectPi"});
    $experiment{"lab"} = "unknown" unless defined($experiment{"lab"});
    $experiment{"labPi"} = "unknown" unless defined($experiment{"labPi"});
    $experiment{"dataType"} = "unknown" unless defined($experiment{"dataType"});
    $experiment{"cell"} = "unknown"  unless defined($experiment{"cell"});
    $experiment{"vars"} = "unknown" unless defined($experiment{"vars"});
    $experiment{"varLabels"} = "unknown" unless defined($experiment{"varLabels"});
    $experiment{"factorLabels"} = "unknown" unless defined($experiment{"factorLabels"});
    $experiment{"submitDate"} = "unknown"  unless defined($experiment{"submitDate"});
    $experiment{"releaseDate"} = "unknown" unless defined($experiment{"releaseDate"});

    # find subId with 'highest' status in current assembly -- use that for reporting
    my $maxId;
    foreach $subId (keys %ids) {
        my $subStat = $submissionStatus{$subId};
        next if (!defined($subStat));
        if (!defined($maxId)) { 
            $maxId = $subId;
            next;
        }
        last if ($submissionStatus{$maxId} eq "released");
        if (Encode::laterPipelineStatus($submissionStatus{$subId}, $submissionStatus{$maxId})) {
            $maxId = $subId;
        }
    }
    # TODO: fix metadata -- for now, exclude if not in status table (e.g. metadata for incorrect assembly)
    if (!defined($maxId)) {
        print STDERR "discarding experiment with no id in this assembly found in project table: " . $key . "\n";
        next;
    }
    $experiment{"status"} = $submissionStatus{$maxId};
    print STDERR "maxID = " . $maxId . "-" .  $experiment{"status"} . "\n";

    # look in pushQ for release date
    my $releaseDate;
    #if (defined($experiment{"objName"}) && $experiment{"status"} eq "released") {
    if ($experiment{"status"} eq "released") {
        print STDERR "Looking up release date for object: " . $experiment{objName} . "\n";
        $releaseDate =
            $dbhPushq->quickQuery("select qadate from pushQ where tbls like ? or files like ? and priority = ?",
                "\%$experiment{objName}\%", "\%$experiment{objName}\%", "L");
        if (!defined($releaseDate)) {
            # fallback 1: Use experiment ID to look up related object in hgsqlbeta's metaDb and
            #                   look that object up in pushQ
            print STDERR "Fallback1 for release date for object: " . $experiment{objName} . "\n";
            my $betaObj =
                $dbhBetaMeta->quickQuery(
                    "select obj from metaDb where var = ? and val = ? limit 1",
                        "expId", "$experiment{expId}");
            if (defined($betaObj)) {
                $releaseDate =
                    $dbhPushq->quickQuery(
                        "select qadate from pushQ where tbls like ? or files like ? and priority = ?",
                            "\%$betaObj\%", "\%$betaObj\%", "L");
            }
        if (!defined($releaseDate)) {
            print STDERR "Fallback2 for release date for object: " . $experiment{objName} . "\n";
            # fallback 2: Use project_status_log date created_at for the ID where status=released
            $releaseDate = $dbhPipeline->quickQuery(
                "select max(created_at) from project_status_logs where project_id = ? and status = ?",
                    $maxId, "released");
            # strip time
            $releaseDate =~ s/ .*//;  
            }
        }
        if (defined($releaseDate)) {
            $experiment{"releaseDate"} = $releaseDate;
        } else {
            $experiment{"releaseDate"} = "unknown";
        }
        print STDERR "Release date for object: " . $experiment{"objName"} . " - " . $releaseDate . "\n";
    }

    # Cosmetics so that auto-generated spreadsheet matches old manual version
    # map dataType tag to term from cv.ra
    $dataType = $experiment{"dataType"};
    if (defined($tags{"dataType"}{$dataType})) {
        $experiment{"dataType"} = 
            (defined($tags{"dataType"}{$dataType}->{"label"})) ?
                $tags{"dataType"}{$dataType}->{"label"} :
                $tags{"dataType"}{$dataType}->{"term"};
    }

    printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%d\t%s\t%s\n", 
                $experiment{"project"}, $experiment{"projectPi"}, 
                $experiment{"lab"}, $experiment{"labPi"}, 
                $experiment{"dataType"}, $experiment{"cell"}, 
                $experiment{"varLabels"}, $experiment{"factorLabels"}, 
                $experiment{"freeze"}, $experiment{"submitDate"}, 
                $experiment{"releaseDate"}, $experiment{"status"}, 
                join(",", sort { $a <=> $b } keys %ids),
                $experiment{"accession"}, $assembly,
                # nhgri report only
                $experiment{"strain"}, $experiment{"age"}, $experiment{"treatment"}, 
                $experiment{"expId"}, $experiment{"dccAccession"}, 
                join(",", sort keys %labIds));
}

exit 0;
