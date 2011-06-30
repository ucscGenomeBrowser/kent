#!/usr/bin/env perl

# encodeReport.pl - use metaDb tables to generate report of submitted and released experiments
#
# Reporting formats:
# ALL: Project, Project-PI, Lab, Lab-PI, Data_Type, Cell_Type, Experiment_Parameters, Experimental_Factors, Freeze, Submit_Date, Release_Date, Status, Submission_IDs, Accession, Assembly, Strain, Age, Treatment, Exp_ID, DCC_Accession

# DCC:       Project(institution), Lab(institution), Data Type, Cell Type, Experiment Parameters, Freeze, Submit_Date, Release_Date, Status, Assembly, Submission Ids, Accession, Exp_ID, DCC_Accession
# Briefly was:
# DCC:       Project(institution), Lab(institution), Data Type, Cell Type, Experiment Parameters, Freeze, Submit_Date, Release_Date, Status, Submission Ids, Accession, Assembly

# NHGRI:     Project(PI), Lab (PI), Assay (TBD), Data Type, Experimental Factor, Organism (human/mouse), Cell Line, Strain, Tissue (TBD), Stage/Age, Treatment, Date Data Submitted, Release Date, Status, Submission Id, GEO/SRA IDs, Assembly, Exp_ID, DCC_Accession

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
    -verbose=num        Set verbose level to num (default 1).
END
exit 1;
}

############################################################################
# Main

my $wd = cwd();

my $ok = GetOptions("configDir=s",
                    "expTable=s",
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

printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", 
        "Project", "Project-PI", "Lab", "Lab-PI", "Data_Type", "Cell_Type", 
        "Experiment_Parameters", "Experimental_Factors", 
        #"Version", 
        "Freeze", "Submit_Date", "Release_Date", 
        "Status", "Submission_IDs", "Accession", "Assembly", 
        "Strain", "Age", "Treatment", "Exp_ID", "DCC_Accession");

# get experiment-defining variables for each composite from the metaDb
open(MDB, "mdbPrint $assembly -table=metaDb -all -line | grep 'objType=composite' |") 
                or die "Can't run mdbPrint\n";
my %metaLines = ();
while (my $line = <MDB>) {
    chomp $line;
    $line =~ s/^.*metadata //;  
    (my $objName, my $settings) = split(" ", $line, 2);
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
    my @varTermTypes;
    my $composite = $metadata{"composite"};

    # can't handle if there are no expVars defined for composite (it's probably a combined or other non-production track)
    my $expVars = $composites{$composite};
    next if (!defined($expVars));
    if (defined($expVars)) {
        @varTermTypes = split(",", $expVars);
    }

    my %vars = ();
    foreach $var (keys %metadata) {
        # special handling for 'treatment=None' and 'age=immortalized' which
        # are defaults
        next if ($var eq "treatment" && $metadata{$var} eq "None");
        next if ($var eq "age" && $metadata{$var} eq "immortalized");
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

    # experimental params -- iterate through all tags, lookup in cv.ra, 
     $experiment{"vars"} = "none";
     $experiment{"expVars"} = "none";
     $experiment{"varLabels"} = "none";
     $experiment{"factorLabels"} = "none";
     $experiment{"treatment"} = "none";
     $experiment{"age"} = "n/a";
     $experiment{"strain"} = "n/a";
     if (scalar(keys %vars) > 0) {
        # alphasort vars by term type, to assure consistency
        # construct vars as list, semi-colon separated
        $experiment{"vars"} = "";
        $experiment{"expVars"} = "";
        $experiment{"varLabels"} = "";
        $experiment{"factorLabels"} = "";
        foreach $termType (sort keys %vars) {
            $var = $vars{$termType};
            $experiment{"vars"} = $experiment{"vars"} .  $termType . "=" . $var . " ";
            if ($termType ne "version" && $var ne "None") {
                $experiment{"expVars"} = $experiment{"expVars"} .  $termType . "=" . $var . " ";
            }
            my $varLabel = defined($terms{$termType}{$var}->{"label"}) ? 
                                $terms{$termType}{$var}->{"label"} : $var;
            # special handling of treatment, age, strain for NHGRI -- they have specific columns
            # treatment is experimental variable; sex is not (for now), age is if not 'immortalized'
            if ($termType eq "treatment") {
                $experiment{"treatment"} = $varLabel;
                $experiment{"varLabels"} = $experiment{"varLabels"} .  $varLabel . ";";
            } elsif ($termType eq "age") {
                next if ($varLabel eq "immortalized");
                $experiment{"age"} = $varLabel;
                next if ($varLabel eq "adult-8wks");
                $experiment{"varLabels"} = $experiment{"varLabels"} .  $varLabel . ";";
            } elsif ($termType eq "strain") {
                $experiment{"strain"} = $varLabel;
            } elsif ($termType eq "sex") {
                next;
            } else {
                # experimental variables
                $experiment{"varLabels"} = $experiment{"varLabels"} .  $varLabel . ";";
                $experiment{"factorLabels"} = $experiment{"factorLabels"} .  ucfirst($termType) . "=" . $varLabel . " ";
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

    # get release date from 'lastUpdated' field of submission dir
    #$experiment{"status"} = "unknown";

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

    # save in a hash of experiments, 
    # (include version in vars)
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

# get submission status for all projects from the pipeline projects table
my %submissionStatus = ();
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
    HgAutomate::verbose(1, "project: \'$name\' $id $status \n");
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

my $releaseDate;

# fill in statuses for those experiments missing them, and print out results
foreach my $key (keys %experiments) {
    my %experiment = %{$experiments{$key}};
    my %ids = %{$experiment{"ids"}};
    print STDERR "    IDs: " . join(",", keys %ids) . " KEY:  " . $key . "\n";
    $experiment{"project"} = "unknown"  unless defined($experiment{"project"});
    $experiment{"projectPi"} = "unknown"  unless defined($experiment{"projectPi"});
    $experiment{"lab"} = "unknown" unless defined($experiment{"lab"});
    $experiment{"labPi"} = "unknown" unless defined($experiment{"labPi"});
    $experiment{"dataType"} = "unknown" unless defined($experiment{"dataType"});
    $experiment{"cell"} = "unknown"  unless defined($experiment{"cell"});
    $experiment{"vars"} = "unknown" unless defined($experiment{"vars"});
    $experiment{"varLabels"} = "unknown" unless defined($experiment{"varLabels"});
    $experiment{"factorLabels"} = "unknown" unless defined($experiment{"factorLabels"});
    $experiment{"treatment"} = "unknown" unless defined($experiment{"treatment"});
    $experiment{"submitDate"} = "unknown"  unless defined($experiment{"submitDate"});
    $experiment{"releaseDate"} = "unknown" unless defined($experiment{"releaseDate"});
    my @ids = sort keys %ids;
    my $lastId = pop @ids;
    # TODO: fix metadata -- for now, exclude if not in status table (e.g. metadata for incorrect assembly)
    if (!defined($submissionStatus{$lastId})) {
        print STDERR "discarding experiment with no status for id (in this assembly): " . $lastId . "\n";
        next;
    }
    $experiment{"status"} = $submissionStatus{$lastId};
    print STDERR "maxID = " . $lastId . "-" .  $experiment{"status"} . "\n";

    #foreach $subId (keys %ids) {
        #last if (defined $experiment{"status"});
        #$experiment{"status"} = defined($submissionStatus{$subId}) ? $submissionStatus{$subId} : "unknown";
        #if ($experiment{"status"} eq "released") {
            #$experiment{"releaseDate"} = $submissionUpdated{$subId}
        #};
    #}
    # look in pushQ for release date
    if (defined($experiment{"objName"}) && $experiment{"status"} eq "released") {
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
                        $lastId, "released");
                # strip time
                $releaseDate =~ s/ .*//;  
            }
        }
    }
    if (defined($releaseDate)) {
        $experiment{"releaseDate"} = $releaseDate;
    } else {
        $experiment{"releaseDate"} = "unknown";
    }
    print STDERR "Release date for object: " . $experiment{"objName"} . " - " . $releaseDate . "\n";

    # Cosmetics so that auto-generated spreadsheet matches old manual version
    # map dataType tag to term from cv.ra
    $dataType = $experiment{"dataType"};
    if (defined($tags{"dataType"}{$dataType})) {
        $experiment{"dataType"} = 
            (defined($tags{"dataType"}{$dataType}->{"label"})) ?
                $tags{"dataType"}{$dataType}->{"label"} :
                $tags{"dataType"}{$dataType}->{"term"};
    }

    printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%d\t%s\n", 
                $experiment{"project"}, $experiment{"projectPi"}, 
                $experiment{"lab"}, $experiment{"labPi"}, 
                $experiment{"dataType"}, $experiment{"cell"}, 
                $experiment{"varLabels"}, $experiment{"factorLabels"}, 
                $experiment{"freeze"}, $experiment{"submitDate"}, 
                $experiment{"releaseDate"}, $experiment{"status"}, 
                join(",", sort keys %ids), 
                $experiment{"accession"}, $assembly,
                # nhgri report only
                $experiment{"strain"}, $experiment{"age"}, $experiment{"treatment"}, 
                $experiment{"expId"}, $experiment{"dccAccession"});
}

exit 0;
