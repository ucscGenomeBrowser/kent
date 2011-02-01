#!/bin/env perl

use strict;
use warnings;

# read two columns from file, tab separated, first column is an integer
# array index, second column is a string to save in that array[index]
sub readTagValue($$) {
    my ($file, $array) = @_;
    if (open (FH, "<$file")) {
#	printf STDERR "# reading $file\n";
	while (my $line = <FH>) {
	    chomp $line;
	    my ($index, $value, $rest) = split('\t',$line, 3);
	    die "more than 2 columns in $file" if (defined($rest));
	    $array->[$index] = $value;
	}
	close (FH);
    }
}

my $argc = scalar(@ARGV);
if ($argc < 2) {
    printf STDERR "usage: ./ncbiIncidentDb.pl -sequenceName <humanSql/chrNN directory> [more directories] > hg19.bed5\n";
    printf STDERR "recognized sequence names: (must be first argument)\n";
    printf STDERR "-GRCh37 == hg19\n";
    printf STDERR "-NCBI36 == hg18\n";
    exit 255;
}

my $sequenceName = shift;
$sequenceName =~ s/^-//;

while (my $dir = shift) {
    my @issueId = [];  # index is autoSql assigned id, value is NCBI key2
    my %issues;   # key is key2, value is an issue structure with attributes
    my @summary = [];  # index is autoSql assigned, id, value is summary string
    my @resolutionText = [];  # index is autoSql assigned, id, value is resolutionText string
    my @description = [];  # index is autoSql assigned, id, value is description string
    my @chr = [];  # index is autoSql assigned, id, value is chr structure
    my $file = "$dir/resolutionText.tab";
    readTagValue($file,\@resolutionText);
    $file = "$dir/description.tab";
    readTagValue($file,\@description);
    $file = "$dir/summary.tab";
    readTagValue($file,\@summary);
    $file = "$dir/chr.tab";
    open (FH, "<$file") or die "can not read $file";
    while (my $line = <FH>) {
	chomp $line;
	my ($id, $text, $gbAcc, $refAcc) = split('\t', $line);
	my %chr;
	$chr{'text'} = $text;
	$chr{'gbAcc'} = $gbAcc;
	$chr{'refAcc'} = $refAcc;
	$chr[$id] = \%chr;
    }
    close (FH);
    $file = "$dir/issue.tab";
    open (FH, "<$file") or die "can not read $file";
#    printf STDERR "# reading $file\n";
    while (my $line = <FH>) {
	chomp $line;
	my ($id, $type, $key2, $chromosome, $accession1, $accession2,
	    $reportType, $summary, $status, $description, $resolutionText)
		= split('\t', $line);
	die "ERROR: id $id already seen in $file" if (exists($issueId[$id]));
	die "ERROR: key $key2 already exists" if (exists($issues{$key2}));
	my %issue;
	$issue{'type'} = $type;
	$issue{'chrom'} = $chromosome;
	$issue{'status'} = $status;
	$issue{'summary'} = $summary;
	$issue{'description'} = $description;
	$issue{'resolutionText'} = $resolutionText;
	$issue{'issueId'} = $id;
	$issueId[$id] = $key2;
	$issues{$key2} = \%issue;
    }
    close (FH);
    my @locationToIssue = [];	# index is location number, value is issue #
    $file = "$dir/issueToLocation.tab";
    open (FH, "<$file") or die "can not read $file";
#    printf STDERR "# reading $file\n";
    while (my $line = <FH>) {
	chomp $line;
	my ($id, $location) = split('\t', $line);
	# construct comma separated list of issues for a single location
	if (defined($locationToIssue[$location])) {
	    my $otherIds = $locationToIssue[$location];
	    $locationToIssue[$location] = sprintf("%s,%d", $otherIds, $id);
	} else {
	    $locationToIssue[$location] = $id;
	}
	my $key2 = $issueId[$id];
	my $issue = $issues{$key2};
	$issue->{'location'} = $location;
# printf STDERR "issueId: %d %s, locationId: %d\n", $id, $key2, $location;
    }
    close (FH);
    $file = "$dir/location.tab";
    open (FH, "<$file") or die "can not read $file";
#    printf STDERR "# reading $file\n";
    while (my $line = <FH>) {
	chomp $line;
	my ($locationId, $name, $gbAsmAcc, $refAsmAcc, $asmStatus, $mapStatus,
	    $chr, $start, $stop, $quality) = split('\t', $line);
#	next if ($name ne "GRCh37");
	die "ERROR: can not find $locationId" if (!exists($locationToIssue[$locationId]));
	my @issueIdList = split(',',$locationToIssue[$locationId]);
	for (my $i = 0; $i < scalar(@issueIdList); ++$i) {
	    my $id = $issueIdList[$i];
	    my $key2 = $issueId[$id];
	    my $issue = $issues{$key2};
	    my %location;
	    $location{'id'} = $locationId;
	    $location{'name'} = $name;
	    $location{'chr'} = $chr[$chr]->{'text'};
	    $location{'start'} = $start;
	    $location{'stop'} = $stop;
	    my $locationKey = sprintf("loc_%s", $name);
	    # maintain comma separated list of location keys
	    if (defined($issue->{'locationKeys'})) {
		$issue->{'locationKeys'} = sprintf("%s,%s",
			$issue->{'locationKeys'}, $locationKey);
	    } else {
		$issue->{'locationKeys'} = $locationKey;
	    }
	    die "locationKey: $locationKey already defined in issue $key2" if (defined($issue->{$locationKey}));
	    $issue->{$locationKey} = \%location;
#    printf STDERR "issueId: %d, locationId: %d asmName: %s\n", $id, $locationId, $name;
	}
    }
    close (FH);
    foreach my $key (keys %issues) {
	my $issue = $issues{$key};
	my @locationKeys = split(',', $issue->{'locationKeys'});
	for (my $i = 0; $i < scalar(@locationKeys); ++$i) {
	    my $location = $issue->{$locationKeys[$i]};
	    my $asmName = $location->{'name'};
	    next if ($sequenceName ne $asmName);
	    my $locationId = $location->{'id'};
	    my $chrLocation = $location->{'chr'};
	    my $start = $location->{'start'};
	    my $stop = $location->{'stop'};
	    next if (($start eq "na") || ($stop eq "na"));
	    my $chr = $issue->{'chrom'};
	    if ($chr ne $chrLocation) {
		printf STDERR "WARN: issue $key chr $chr != $chrLocation chr location\n";
	    }
	    next if ($chrLocation =~ m/\|/);
	    my $summary = $summary[$issue->{'summary'}];
	    $summary =~ s/\s+$//;
	    $summary =~ s/\s/&nbsp;/g;
	    my $description = $description[$issue->{'description'}];
	    $description =~ s/\s+$//;
	    $description =~ s/\s/&nbsp;/g;
	    my $resolutionText = $issue->{'resolutionText'};
	    if (scalar(@resolutionText) > 0 && $resolutionText =~ m/^[0-9]+$/) {
		$resolutionText = $resolutionText[$resolutionText];
	    }
	    $resolutionText =~ s/\s+$//;
	    $resolutionText =~ s/\s/&nbsp;/g;
	    $issue->{'status'} =~ s/\s+$//;
	    $issue->{'status'} =~ s/\s/&nbsp;/g;
	    my $detailPage=sprintf("<BR><B>Status:&nbsp;</B>%s<BR>", $issue->{'status'});
	    $detailPage=sprintf("%s<B>Summary:&nbsp;</B>%s<BR>",
		$detailPage, $summary);
	    $detailPage=sprintf("%s<B>Description:&nbsp;</B>%s<BR>",
		$detailPage, $description);
	    $detailPage=sprintf("%s<B>Resolution:&nbsp;</B>%s<BR>",
		$detailPage, $resolutionText);
	    printf "chr%s\t%d\t%d\t%s\t%s\n", $chrLocation, $start, $stop, $key,
		    $detailPage;
	}
    }
}
