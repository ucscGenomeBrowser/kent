#!/usr/bin/env perl

# encodeReport.pl - use metaDb tables to generate report of submitted and released experiments
#
# Reporting formats:
# ALL: Project, Project-PI, Lab, Lab-PI, Data_Type, Cell_Type, Experiment_Parameters, Experimental_Factors, Freeze, Submit_Date, Release_Date, Status, Submission_IDs, Accession, Assembly, Strain, Age, Treatment);
# DCC:       Project(institution), Lab(institution), Data Type, Cell Type, Experiment Parameters, Freeze, Submit_Date, Release_Date, Status, Assembly, Submission Ids, Accession
# Briefly was:
# DCC:       Project(institution), Lab(institution), Data Type, Cell Type, Experiment Parameters, Freeze, Submit_Date, Release_Date, Status, Submission Ids, Accession, Assembly
# NHGRI:     Project(PI), Lab (PI), Assay (TBD), Data Type, Experimental Factor, Organism (human/mouse), Cell Line, Strain, Tissue (TBD), Stage/Age, Treatment, Date Data Submitted, Release Date, Status, Submission Id, GEO/SRA IDs, Assembly

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
our $configPath = "/cluster/home/kate/kent/src/hg/encode/encodeReport/config";
our $assembly;  # required command-line arg

sub usage {
    print STDERR <<END;
usage: encodeReport.pl <assembly>

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

if(!(-d $configPath)) {
    die "configPath '$configPath' is invalid; Can't find the config directory\n";
}
HgAutomate::verbose(4, "Config directory path: \'$configPath\'\n");

my %terms = Encode::getControlledVocab($configPath);
my %tags = Encode::getControlledVocabTags($configPath);
my @termTypes = (keys %terms);

# use pi.ra file to map pi/lab/institution/grant/project
my $labRef = Encode::getLabs($configPath);
my %labs = %{$labRef};
my $dataType;
my %dataTypes = %{$terms{"dataType"}};
my $var;
my $termType;
my $subId;

printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", 
        "Project", "Project-PI", "Lab", "Lab-PI", "Data_Type", "Cell_Type", 
        "Experiment_Parameters", "Experimental_Factors", 
        #"Version", 
        "Freeze", "Submit_Date", "Release_Date", 
        "Status", "Submission_IDs", "Accession", "Assembly", 
        "Strain", "Age", "Treatment");
my %metaLines = ();

# print metadata from table in 'lines' format
open(MDB, "mdbPrint $assembly -table=metaDb -all -line |") or die "Can't run mdbPrint\n";
while (my $line = <MDB>) {
    chomp $line;
    $line =~ s/^.*metadata //;  
    (my $objName, my $settings) = split(" ", $line, 2);
    #$line =~ s/objType=table //;  
    #$line =~ s/objType=file //;  
    $metaLines{$objName} = $settings;
    print STDERR "     OBJNAME: $objName   SETTINGS: $settings\n";
}

# Create experiments hash
my %experiments = ();

foreach my $objName (keys %metaLines) {
    my $settings = $metaLines{$objName};
    print STDERR "     SETTINGS: $settings\n";
    my $ref = Encode::metadataLineToHash($settings);
    my %metadata =  %{$ref};

    # short-circuit if this didn't come in from pipeline
    next unless defined($metadata{"subId"});

    my %experiment = ();
    $experiment{"project"} = "unknown";
    $experiment{"projectPi"} = "unknown";
    $experiment{"labPi"} = "unknown";
    $experiment{"objName"} = $objName;

    my $lab = "unknown";
    if (defined($metadata{"lab"})) {
        $lab = $metadata{"lab"};
        # strip off PI name in parens
        $lab =~ s/\(\w+\)//;
        # TODO: Fix 'lab=' metadata for UT-A to be consistent
        $lab = "UT-A" if ($lab eq "UT-Austin");
    }
    foreach $var (keys %labs) {
        if (lc($var) eq lc($lab)) {
            $lab = $var;
            $experiment{"labPi"} = $labs{$lab}->{"pi"};
            $experiment{"project"} = $labs{$lab}->{"project"};
            $experiment{"projectPi"} = $labs{$lab}->{"grant"};
            warn "lab/project mismatch in settings $settings" 
                    unless ($metadata{"grant"} eq $experiment{"projectPi"});
        }
    }
    $experiment{"lab"} = $lab;
    if ($experiment{"project"} eq "unknown") {
        warn "lab $lab not found in pi.ra, from settings $settings";
    }
    # for now, force all Yale projects to be Yale lab (until metadata in projects table can match)
    #if ($metadata{"grant"} eq "Snyder") {
        #$experiment{"lab"} = "Yale";
    #}

    # look up dataType case-insensitive, as tag or term, save as tag
    my $dType = $metadata{"dataType"};
    $dataType = $dType;
    foreach $var (keys %dataTypes) {
        if (lc($dType) eq lc($var) || lc($dType) eq lc($dataTypes{$var}->{"tag"})) {
            $dataType = $dataTypes{$var}->{"tag"};
        }
    }
    #$dataType = $metadata{"dataType"};
    #if (!defined($tags{"dataType"}{$dataType})) {
        # get tag if term was used
        #foreach $var (keys %dataTypes) {
            #if ($var eq $dataType) {
                #$dataType = $terms{"dataType"}{$var}->{"tag"};
            #}
        #}
    #}
    $experiment{"dataType"} = $dataType;

    $experiment{"cell"} = "none";
    if (defined($metadata{"cell"})) {
        $experiment{"cell"} = $metadata{"cell"};
    }
    print STDERR "    objName  " . $objName . "\n";

    # determine term type for experiment parameters 
    my %vars = ();

    foreach $var (keys %metadata) {
        foreach $termType (@termTypes) {
            # skip special terms
            next if ($termType eq "Cell Line");
            next if ($termType eq "dataType");
            next if ($termType eq "project");
            next if ($termType eq "seqPlatform");
            next if ($termType eq "grant");
            next if ($termType eq "lab");
            #next if ($termType eq "control");
            if (lc($termType) eq lc($var) && defined($terms{$termType}{$metadata{$var}})) {
                $vars{$termType} = $metadata{$var};
            }
        }
    }

    my @varList = keys %vars;
    #print STDERR "VARS: " . "@varList" . "\n";
    # treat version like a variable
    if (defined($metadata{"submittedDataVersion"})) {
        my $version = $metadata{"submittedDataVersion"};
        # strip comment
        $version =~ s/^.*(V\d+).*/$1/;
        $vars{"version"} = $version;
    }

    # collapse multiple terms for GEO accession
    if (defined($metadata{"accession"})) {
        $experiment{"accession"} = $metadata{"accession"};
    } elsif (defined($metadata{"geoSampleAccession"})) {
        $experiment{"accession"} = $metadata{"geoSampleAccession"};
    } elsif (defined($metadata{"geoSeriesAccession"})) {
        $experiment{"accession"} = $metadata{"geoSeriesAccession"};
    } else {
        $experiment{"accession"} = "";
    }

    # experimental params -- iterate through all tags, lookup in cv.ra, 
     $experiment{"vars"} = "none";
     $experiment{"varLabels"} = "none";
     $experiment{"factorLabels"} = "none";
     $experiment{"treatment"} = "none";
     $experiment{"age"} = "n/a";
     $experiment{"strain"} = "n/a";
    if (scalar(keys %vars) > 0) {
        # alphasort vars by term type, to assure consistency
        # TODO: define explicit ordering for term type in cv.ra
        # construct vars as list, semi-colon separated
        $experiment{"vars"} = "";
        $experiment{"varLabels"} = "";
        $experiment{"factorLabels"} = "";
        foreach $termType (sort keys %vars) {
            $var = $vars{$termType};
            $experiment{"vars"} = $experiment{"vars"} .  $var . ";";
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

    #die "undefined project" unless defined($experiment{"project"});
    #die "undefined lab" unless defined($experiment{"lab"});
    #die "undefined dataType" unless defined($experiment{"dataType"});
    #die "undefined cell" unless defined($experiment{"cell"});
    #die "undefined vars" unless defined($experiment{"vars"});
    #die "undefined freeze" unless defined($experiment{"freeze"});
    #die "undefined submitDate" unless defined($experiment{"submitDate"});
    #die "undefined releaseDate" unless defined($experiment{"releaseDate"});
    #die "undefined status" unless defined($experiment{"status"});

    # create key for experiment: lab+dataType+cell+vars
    my $expKey = $experiment{"lab"} . "+" . $experiment{"dataType"} . "+" . 
                                $experiment{"cell"} . "+" . 
                                $experiment{"vars"};

    print STDERR "KEY: " . $expKey . "\n";

    # save in a hash of experiments, 
    #   keyed by lab+dataType+cell+vars
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
my $dbh = HgDb->new(DB => $pipeline);
my $sth = $dbh->execute( 
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
$dbh->{DBH}->disconnect;

# open pushQ to extract release dates
our $pushqDb = "qapushq";
our $pushqHost = "mysqlbeta";
my $releaseDate;
$dbh = HgDb->new(DB => $pushqDb, HOST => $pushqHost);

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
    $experiment{"strain"} = "unknown" unless defined($experiment{"strain"});
    $experiment{"age"} = "unknown" unless defined($experiment{"age"});
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
        print STDERR "looking up release date for object: " . $experiment{objName} . "\n";
        $releaseDate =
            $dbh->quickQuery("select qadate from pushQ where tbls like ? or files like ? and priority = ?",
                "\%$experiment{objName}\%", "\%$experiment{objName}\%", "L");
        if (defined($releaseDate)) {
            $experiment{"releaseDate"} = $releaseDate;
        } else {
            print STDERR "****  NOT FOUND\n";
            }
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
    # map lab to label in pi.ra
    my $lab = $experiment{"lab"};
    if (defined($labs{$lab}->{"label"})) {
        $experiment{"lab"} = $labs{$lab}->{"label"};
    }

    printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", 
                $experiment{"project"}, $experiment{"projectPi"}, 
                $experiment{"lab"}, $experiment{"labPi"}, 
                $experiment{"dataType"}, $experiment{"cell"}, 
                $experiment{"varLabels"}, $experiment{"factorLabels"}, 
                $experiment{"freeze"}, $experiment{"submitDate"}, 
                $experiment{"releaseDate"}, $experiment{"status"}, 
                join(",", sort keys %ids), 
                $experiment{"accession"}, $assembly,
                # nhgri report only
                $experiment{"strain"}, $experiment{"age"}, $experiment{"treatment"});
}

exit 0;
