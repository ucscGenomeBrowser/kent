#
# Encode: support routines for ENCODE scripts
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/Encode.pm instead.
#
# $Id: Encode.pm,v 1.12 2008/08/25 22:26:38 larrym Exp $

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
our $dafVersion = "0.2.2";

our $fieldConfigFile = "fields.ra";
our $vocabConfigFile = "cv.ra";
our $labsConfigFile = "labs.ra";

our $sqlCreate = "/cluster/bin/sqlCreate";
# Add type names to this list for types that can be loaded via .sql files (e.g. bed5FloatScore.sql)
# You also have to make sure the .sql file is copied into the $sqlCreate directory.
our @extendedTypes = ("narrowPeak", "broadPeak", "gappedPeak", "tagAlign", "pairedTagAlign", "bed5FloatScore");

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
# validate the entries in a RA record or DDF header using fields.ra
# $file s/d be 'ddf' or 'dafHeader'
# die's if any errors are found.
    my ($fields, $schema, $file, $errStrPrefix) = @_;
    my %hash = map {$_ => 1} @{$fields};
    my @errors;
    die "file '$file' is invalid\n" if($file ne 'ddf' && $file ne 'dafHeader');

    # look for missing required fields
    for my $field (keys %{$schema}) {
        if($schema->{$field}{file} eq $file && $schema->{$field}{required} && !defined($hash{$field})) {
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
        die $errStrPrefix . " " . join("; ", @errors) . " \n";
    }
}

sub validateValueList {
# validate hash of values using fields.ra; $file s/d be 'ddf' or 'dafHeader'
# die's if any errors are found.
    my ($values, $schema, $file, $errStrPrefix) = @_;
    my @errors;
    for my $field (keys %{$values}) {
        my $val = $values->{$field};
        if(defined($schema->{$field}{file}) && $schema->{$field}{file} eq $file) {
            my $type = $schema->{$field}{type} || 'string';
            if($type eq 'bool') {
                if(lc($val) ne 'yes' && lc($val) ne 'no') {
                    push(@errors, "invalid boolean; field '$field', value '$val'; value must be 'yes' or 'no'");
                } else {
                    $values->{$field} = lc($val) eq 'yes' ? 1 : 0;
                }
            } elsif($type eq 'int') {
                if($val !~ /^\d+$/) {
                    push(@errors, "invalid integer; field '$field', value '$val'");
                }
            }
        }
    }
    if(@errors) {
        die $errStrPrefix . " " . join("; ", @errors) . " \n";
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

sub getGrants
{
# The grants are called "labs" in the labs.ra file (for historical reasons).
    my ($configPath) = @_;
    my %grants;
    if(-e "$configPath/$labsConfigFile") {
        # tolerate missing labs.ra in dev trees.
        %grants = RAFile::readRaFile("$configPath/$labsConfigFile", "lab");
    }
    return \%grants;
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
    my %fields = RAFile::readRaFile("$configPath/$fieldConfigFile", "field");

    # For convenience, convert "required" to a real boolean (1 or 0);
    for my $key (keys %fields) {
        if(exists($fields{$key}->{required})) {
            my $val = $fields{$key}->{required};
            $fields{$key}->{required} = lc($val) eq 'yes' ? 1 : 0;
        }
    }
    return \%fields;
}

sub validateAssembly {
    my ($val) = @_;
    if($val ne 'hg18') {
        die "ERROR: Assembly '$val' is invalid (must be 'hg18')\n";
    }
}

sub getDaf
{
# Return reference to DAF hash, using newest DAF file found in $submitDir.
# hash keys are RA style plus an additional TRACKS key which is a nested hash for
# the track list at the end of the DAF file; e.g.:
# (lab => 'Myers', TRACKS => {'Alignments => {}, Signal => {}})
    my ($submitDir, $grants, $fields) = @_;

    # Read info from Project Information File.  Verify required fields
    # are present and that the project is marked active.
    my %daf = ();
    $daf{TRACKS} = {};
    my $wd = cwd();
    chdir($submitDir);
    my @glob = glob "*.DAF";
    push(@glob, glob "*.daf");
    my $dafFile = newestFile(@glob);
    if(!(-e $dafFile)) {
        die "Can't find the DAF file\n";
    }
    &HgAutomate::verbose(2, "Using newest DAF file \'$dafFile\'\n");
    my $lines = readFile("$dafFile");
    chdir($wd);

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
        if ($key eq "view") {
            my %track = ();
            my $track = $val;
            $daf{TRACKS}->{$track} = \%track;
            &HgAutomate::verbose(5, "  Found view: \'$track\'\n");
            while ($line = shift @{$lines}) {
                $line =~ s/^ +//;
                $line =~ s/ +$//;
                next if $line =~ /^#/;
                next if $line =~ /^$/;
                if ($line =~ /^view/) {
                    unshift @{$lines}, $line;
                    last;
                }
                my ($key, $val) = splitKeyVal($line);
                $track{$key} = $val;
                &HgAutomate::verbose(5, "    Property: $key = $val\n");
            }
            $track{required} = lc($track{required}) eq 'yes' ? 1 : 0;
            $track{hasReplicates} = lc($track{hasReplicates}) eq 'yes' ? 1 : 0;
        } else {
            &HgAutomate::verbose(3, "DAF field: $key = $val\n");
            $daf{$key} = $val;
        }
    }

    # Validate fields
    my @tmp = grep(!/^TRACKS$/, keys %daf);
    validateFieldList(\@tmp, $fields, 'dafHeader', "ERROR in DAF '$dafFile':");

    if($daf{dafVersion} ne $dafVersion) {
        die "ERROR: dafVersion '$daf{dafVersion}' does not match current version: $dafVersion\n";
    }
    if(!keys(%{$daf{TRACKS}})) {
        die "ERROR: no views defined for project \'$daf{project}\' in DAF '$dafFile'\n";
    }
    if(!defined($grants->{$daf{grant}})) {
        die "ERROR: invalid lab '$daf{grant}' in DAF '$dafFile'\n";
    }
    validateAssembly($daf{assembly});

    foreach my $track (keys %{$daf{TRACKS}}) {
        &HgAutomate::verbose(4, "  Track: $track\n");
        my %track = %{$daf{TRACKS}->{$track}};
        foreach my $key (keys %track) {
            &HgAutomate::verbose(4, "    Setting: $key   Value: $track{$key}\n");
        }
    }

    if (defined($daf{variables})) {
        my @variables = split (/\s*,\s*/, $daf{variables});
        my %variables;
        my $i = 0;
        foreach my $variable (@variables) {
            # replace underscore with space
            $variable =~ s/_/ /g;
            $variables[$i++] = $variable;
            $variables{$variable} = 1;
        }
        $daf{variableHash} = \%variables;
        $daf{variableArray} = \@variables;
    }
    return \%daf;
}

sub compositeTrackName
{
    my ($daf) = @_;
    return "wgEncode$daf->{lab}$daf->{dataType}";
}

sub downloadDir
{
    my ($daf) = @_;
    return "/usr/local/apache/htdocs/goldenPath/$daf->{assembly}/" . compositeTrackName($daf);
}

1;
