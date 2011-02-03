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
    printf STDERR "-MGSCv37 == mm9\n";
    printf STDERR "-Zv9 == danRer7\n";
    exit 255;
}

my $sequenceName = shift;
$sequenceName =~ s/^-//;

while (my $dir = shift) {
    my @issueId;  # index is autoSql assigned id, value is NCBI key2
    my %issues;   # key is key2, value is an issue structure with attributes
    my @summary;  # index is autoSql assigned, id, value is summary string
    my @resolutionText;  # index is autoSql assigned, id, value is resolutionText string
    my @description;  # index is autoSql assigned, id, value is description string
    my @chr;  # index is autoSql assigned, id, value is chr structure
    # read in some files that are just two columns, an id and a string
    # save in arrays
    my $file = "$dir/resolutionText.tab";
    readTagValue($file,\@resolutionText);
    $file = "$dir/description.tab";
    readTagValue($file,\@description);
    $file = "$dir/summary.tab";
    readTagValue($file,\@summary);
    #	this file has three or four columns.  Save as structure in array
    $file = "$dir/chr.tab";
    open (FH, "<$file") or die "can not read $file";
    while (my $line = <FH>) {
	chomp $line;
	my $id = 0;
	my $text = "";
	my $gbAcc = "";
	my $refAcc = "";
	my @a = split('\t', $line);
	if (scalar(@a) == 4) {
	    ($id, $text, $gbAcc, $refAcc) = split('\t', $line);
	} elsif (scalar(@a) == 3) {
	    ($text, $gbAcc, $refAcc) = split('\t', $line);
	} elsif (scalar(@a) < 3) {
	    next;
	} else {
	    printf STDERR "column count: %d in $file\n", scalar(@a);
	    printf STDERR "line: '%s'\n", $line;
	    die "can not recognize 3 or 4 columns in $file";
	}
	if ($id eq "na") {
	    printf STDERR "WARN: discarding from $file: '$line'\n";
	    next;
	}
	my %chr;
	$chr{'text'} = $text;
	$chr{'gbAcc'} = $gbAcc;
	$chr{'refAcc'} = $refAcc;
	$chr[$id] = \%chr;
    }
    close (FH);

    my @locationToIssue;	# index is location number, value is issue #
    # mouse and zebrafish have location index in the issue record
    # human has multiple locations in each issue and has
    #	an issueToLocation table
    $file = "$dir/issue.tab";
    open (FH, "<$file") or die "can not read $file";
#    printf STDERR "# reading $file\n";
    while (my $line = <FH>) {
	chomp $line;
	my ($id, $type, $key2, $chromosome, $accession1, $accession2,
	    $reportType, $summary, $status, $description, $resolutionText,
		$location) = split('\t', $line);
	die "ERROR: id $id already seen in $file" if (exists($issueId[$id]));
	die "ERROR: key $key2 already exists" if (exists($issues{$key2}));
	my %issue;
	$issue{'type'} = $type;
	$issue{'chrom'} = $chromosome;
	$issue{'status'} = $status;
	$issue{'reportType'} = $reportType;
	$issue{'summary'} = $summary;
	$issue{'description'} = $description;
	$issue{'resolutionText'} = $resolutionText;
	$issue{'issueId'} = $id;
	if (defined($location)) {
	    $issue{'location'} = $location;
	    if (defined($locationToIssue[$location])) {
		my $otherIds = $locationToIssue[$location];
		$locationToIssue[$location] = sprintf("%s,%d", $otherIds, $id);
	    } else {
		$locationToIssue[$location] = $id;
	    }
	}
	$issueId[$id] = $key2;
	$issues{$key2} = \%issue;
    }
    close (FH);
    # issueToLocation does not exist for mouse and zebrafish
    $file = "$dir/issueToLocation.tab";
    if (open (FH, "<$file")) {
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
    }
    #	zebrafish sometimes only has one location and it does not
    #	have a locationId column in this file
    $file = "$dir/location.tab";
    open (FH, "<$file") or die "can not read $file";
    printf STDERR "# reading $file\n";
    while (my $line = <FH>) {
	chomp $line;
	my @a = split('\t', $line);
	my $locationId = "na";
	my $name = "";
	my $gbAsmAcc = "";
	my $refAsmAcc = "";
	my $asmStatus = "";
	my $mapStatus = "";
	my $chr = "na";
	my $start = 0;
	my $stop = 0;
	my $quality = "";
	if (scalar(@a) == 10) {
	    ($locationId, $name, $gbAsmAcc, $refAsmAcc, $asmStatus, $mapStatus,
		$chr, $start, $stop, $quality) = split('\t', $line);
	} elsif (scalar(@a) == 9) {
	    ($name, $gbAsmAcc, $refAsmAcc, $asmStatus, $mapStatus,
		$chr, $start, $stop, $quality) = split('\t', $line);
	} else {
	    die "can not recognize 9 or 10 columns in location.tab";
	}
	if ($chr eq "na") {
	    printf STDERR "WARN: discarding from $file: '$line'\n";
	    next;
	}
	$locationId = 0 if ($locationId eq "na");

	die "ERROR: can not find $locationId" if (!exists($locationToIssue[$locationId]));
	my @issueIdList = split(',',$locationToIssue[$locationId]);
	for (my $i = 0; $i < scalar(@issueIdList); ++$i) {
	    my $id = $issueIdList[$i];
	    my $key2 = $issueId[$id];
	    my $issue = $issues{$key2};
	    my %location;
	    $location{'id'} = $locationId;
	    $location{'name'} = $name;
	    if (($chr =~ m/^[0-9]+$/) && defined($chr[$chr])) {
		$location{'chr'} = $chr[$chr]->{'text'};
	    } else {
		$location{'chr'} = $chr;
	    }
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
	if (!defined($issue->{'locationKeys'})) {
	    printf STDERR "WARN: discarding $key, no locationKeys\n";
	    next;
	}
	my @locationKeys = split(',', $issue->{'locationKeys'});
	for (my $i = 0; $i < scalar(@locationKeys); ++$i) {
	    my $location = $issue->{$locationKeys[$i]};
	    my $asmName = $location->{'name'};
	    next if ($sequenceName ne $asmName);
	    my $locationId = $location->{'id'};
	    my $chrLocation = $location->{'chr'};
	    my $start = $location->{'start'};
	    my $stop = $location->{'stop'};
	    if (($start eq "na") || ($stop eq "na")) {
		printf STDERR "WARN: discarding $key, chrLocation: $chrLocation\n";
	    }
	    next if (($start eq "na") || ($stop eq "na"));
	    my $chr = $issue->{'chrom'};
	    if ($chr ne $chrLocation) {
		printf STDERR "WARN: issue $key chr $chr != $chrLocation chr location\n";
		$chrLocation = $chr if ($chrLocation =~ m/CU/);
	    }
	    if ( ($chrLocation =~ m/\|/) || ($chrLocation =~ m/un|na/i)) {
		printf STDERR "WARN: discarding $key, chrLocation: $chrLocation\n";
		next;
	    }
	    # @summary may have no elements, summary text is in issue
	    my $summary = $issue->{'summary'};
	    if ($summary =~ m/^[0-9]+$/ && defined($summary[$summary])) {
		$summary = $summary[$summary];
	    }
	    $summary =~ s/\s+$//;
	    my $description = $description[$issue->{'description'}];
	    $description =~ s/\s+$//;
	    my $resolutionText = $issue->{'resolutionText'};
	    if (scalar(@resolutionText) > 0 && $resolutionText =~ m/^[0-9]+$/) {
		$resolutionText = $resolutionText[$resolutionText];
	    }
	    $resolutionText =~ s/\s+$//;
	    $issue->{'status'} =~ s/\s+$//;
	    $issue->{'type'} =~ s/\s+$//;
	    my $detailPage=sprintf("<BR><B>Category:&nbsp;</B>%s<BR>",
		$issue->{'type'});
	    $detailPage=sprintf("%s<B>Report&nbsp;type:&nbsp;</B>%s<BR>",
		$detailPage, $issue->{'reportType'});
	    $detailPage=sprintf("%s<B>Status:&nbsp;</B>%s<BR>",
		$detailPage, $issue->{'status'});
	    $detailPage=sprintf("%s<B>Summary:&nbsp;</B>%s<BR>",
		$detailPage, $summary);
	    $detailPage=sprintf("%s<B>Description:&nbsp;</B>%s<BR>",
		$detailPage, $description);
	    $detailPage=sprintf("%s<B>Resolution:&nbsp;</B>%s<BR>",
		$detailPage, $resolutionText);
	    $detailPage =~ s/\s/&nbsp;/g;
	    printf "chr%s\t%d\t%d\t%s\t%s\n", $chrLocation, $start-1, $stop-1, $key,
		    $detailPage;
	}
    }
}
