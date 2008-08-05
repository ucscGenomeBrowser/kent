#
# Encode: support routines for ENCODE scripts
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/Encode.pm instead.
#
# $Id: Encode.pm,v 1.4 2008/08/05 00:09:12 larrym Exp $

package Encode;

use warnings;
use strict;

use File::stat;
use Cwd;

use RAFile;
use HgAutomate;

# Global constants
our $loadFile = "load.ra";
our $unloadFile = "unload.ra";
our $trackFile = "trackDb.ra";
our $pushQFile = "pushQ.sql";

my $fieldConfigFile = "fields.ra";
my $vocabConfigFile = "cv.ra";
my $labsConfigFile = "labs.ra";
my $pifVersion = 0.2;

sub newestFile
{
  # Get the most recently modified file from a list
    my @files = @_;
    my $newestTime = 0;
    my $newestFile = "";
    my $file = "";
    foreach $file (@files) {
        my $fileTime = (stat($file))->mtime;
        if ($fileTime > $newestTime) {
            $newestTime = $fileTime;
            $newestFile = $file;
        }
    }
    return $newestFile;
}

sub splitKeyVal
{
# split a line into key/value, using the FIRST white-space in the line; we also trim key/value strings
    my ($str) = @_;
    my $key = undef;
    my $val = undef;
    if($str =~ /([^\s]+)\s+(.+)/) {
        $key = $1;
        $val = $2;
        $key =~ s/^\s+//;
        $key =~ s/\s+$//;
        $val =~ s/^\s+//;
        $val =~ s/\s+$//;
    }
    return ($key, $val);
}


sub validateFieldList {
# validate the entries in a RA record or DDF header using labs.ra as our schema
    my ($fields, $schema, $file, $errStrSuffix) = @_;
    my %hash = map {$_ => 1} @{$fields};
    my @errors;

    # look for missing required fields
    for my $field (keys %{$schema}) {
        if($schema->{$field}{file} eq $file && $schema->{$field}{required} eq 'yes' && !defined($hash{$field})) {
            push(@errors, "field '$field' not defined");
        }
    }

    # now look for fields in list that aren't in schema
    for my $field (@{$fields}) {
        if(!defined($schema->{$field}{file}) || $schema->{$field}{file} ne $file) {
            push(@errors, "invalid field '$field'");
        }
    }
    if(@errors) {
        die "ERROR: " . join("; ", @errors) . " $errStrSuffix\n";
    }
}

sub readFile
{
# Return lines from given file, with EOL chomp'ed off.
# Handles either Unix or Mac EOL characters.
# Reads whole file into memory, so should NOT be used for huge files.
    my ($file) = @_;
    my $oldEOL = $/;
    open(FILE, $file) or die "ERROR: Can't open file \'$file\'\n";
    my @lines = <FILE>;
    if(@lines == 1 && $lines[0] =~ /\r/) {
        # rewind and re-read as a Mac file - obviously, this isn't the most efficient way to do this.
        seek(FILE, 0, 0);
        $/ = "\r";
        @lines = <FILE>;
    }
    for (@lines) {
        chomp;
    }
    close(FILE);
    $/ = $oldEOL;
    return \@lines;
}

sub getLabs
{
    my ($configPath) = @_;
    my %labs;
    if(-e "$configPath/$labsConfigFile") {
        # tolerate missing labs.ra in dev trees.
        %labs = RAFile::readRaFile("$configPath/$labsConfigFile", "lab");
    }
    return %labs;
}

sub getControlledVocab
{
# Returns hash indexed by the type's in the cv.ra file (e.g. "Cell Line", "Antibody")
    my ($configPath) = @_;
    my %terms = ();
    my %termRa = RAFile::readRaFile("$configPath/$vocabConfigFile", "term");
    foreach my $term (keys %termRa) {
        my $type = $termRa{$term}->{type};
        $terms{$type}->{$term} = $termRa{$term};
    }
    return %terms;
}

sub getFields
{
# Gather fields defined for DDF file. File is in 
# ra format:  field <name>, required <true|false>
    my ($configPath) = @_;
    return RAFile::readRaFile("$configPath/$fieldConfigFile", "field");
}

sub validateAssembly {
    my ($val) = @_;
    $val =~ /^hg1[78]$/ || die "ERROR: Assembly '$val' is invalid (must be 'hg17' or 'hg18')\n";
}

sub getPif
{
# Return PIF hash, using newest PIF file found in $submitDir.
# hash keys are RA style plus an additional TRACKS key which is a nested hash for
# the track list at the end of the PIF file; e.g.:
# (lab => 'Myers', TRACKS => {'Alignments => {}, Signal => {}})
    my ($submitDir, $labs, $fields) = @_;

    # Read info from Project Information File.  Verify required fields
    # are present and that the project is marked active.
    my %pif = ();
    $pif{TRACKS} = {};
    my $wd = cwd();
    chdir($submitDir);
    my $pifFile = newestFile(glob "*.PIF");
    &HgAutomate::verbose(2, "Using newest PIF file \'$pifFile\'\n");
    chdir($wd);

    my $lines = readFile("$submitDir/$pifFile");
    while (@{$lines}) {
        my $line = shift @{$lines};
        # strip leading and trailing spaces
        $line =~ s/^ +//;
        $line =~ s/ +$//;
        # ignore comments and blank lines
        next if $line =~ /^#/;
        next if $line =~ /^$/;

        my ($key, $val) = splitKeyVal($line);
        if(!defined($key)) {
            next;
        }
        if ($key ne "view") {
            &HgAutomate::verbose(3, "PIF field: $key = $val\n");
            $pif{$key} = $val;
        } else {
            my %track = ();
            my $track = $val;
            $pif{TRACKS}->{$track} = \%track;
            &HgAutomate::verbose(5, "  Found view: \'$track\'\n");
            while ($line = shift @{$lines}) {
                $line =~ s/^ +//;
                $line =~ s/ +$//;
                next if $line =~ /^#/;
                next if $line =~ /^$/;
                if ($line =~ /^track/) {
                    unshift @{$lines}, $line;
                    last;
                }
                my ($key, $val) = splitKeyVal($line);
                $track{$key} = $val;
                &HgAutomate::verbose(5, "    Property: $key = $val\n");
            }
        }
    }

    # Validate fields
    my @tmp = grep(!/^TRACKS$/, keys %pif);
    validateFieldList(\@tmp, $fields, 'pifHeader', "in PIF '$pifFile'");

    if($pif{pifVersion} ne $pifVersion) {
        die "ERROR: pifVersion '$pif{pifVersion}' does not match current version: $pifVersion\n";
    }
    if(!keys(%{$pif{TRACKS}})) {
        die "ERROR: no views defined for project \'$pif{project}\' in PIF '$pifFile'\n";
    }
    if(!defined($labs->{$pif{grant}})) {
        die "ERROR: invalid lab '$pif{grant}' in PIF '$pifFile'\n";
    }
    validateAssembly($pif{assembly});

    foreach my $track (keys %{$pif{TRACKS}}) {
        &HgAutomate::verbose(4, "  Track: $track\n");
        my %track = %{$pif{TRACKS}->{$track}};
        foreach my $key (keys %track) {
            &HgAutomate::verbose(4, "    Setting: $key   Value: $track{$key}\n");
        }
    }

    if (defined($pif{'variables'})) {
        my @variables = split (/\s*,\s*/, $pif{'variables'});
        my %variables;
        my $i = 0;
        foreach my $variable (@variables) {
            # replace underscore with space
            $variable =~ s/_/ /g;
            $variables[$i++] = $variable;
            $variables{$variable} = 1;
        }
        $pif{'variableHash'} = \%variables;
        $pif{'variableArray'} = \@variables;
    }
    return %pif;
}

1;
