#!/usr/bin/env perl

use strict;
use warnings;

# set debug to 1 to see more output to stderr
my $debug = 0;

# given database, table, tab file, load file into db.table
sub loadTmpTable($$$) {
    my ($db, $table, $tabFile) = @_;
    my $sqlFile = $tabFile;
    $sqlFile =~ s/.tab$/.sql/;
    `/cluster/bin/x86_64/hgsql -e "drop table if exists $table;" $db`;
    die "ERROR: failed drop table $db.$table" if ($?);
    `/cluster/bin/x86_64/hgsql $db < $sqlFile`;
    die "ERROR: failed create table $db.$table from $sqlFile" if ($?);
    `/cluster/bin/x86_64/hgsql -e "load data local infile '$tabFile' into table $table;" $db`;
    die "ERROR: failed load data table $db.$table from $tabFile" if ($?);
}


# read two columns from file, tab separated, first column is an integer
# array index, second column is a string to save in that array[index]
sub readTagValue($$) {
    my ($file, $array) = @_;
    if (open (FH, "<$file")) {
	printf STDERR "# reading $file\n" if ($debug);
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
    printf STDERR "usage: ./grcIncidentDb.pl -sequenceName <humanSql/chrNN directory> [more directories] > hg19.bed5\n";
    printf STDERR "recognized sequence names: (must be first argument)\n";
    printf STDERR "-GRCh37 == hg19\n";
    printf STDERR "-GRCh38 == hg38\n";
    printf STDERR "-NCBI36 == hg18\n";
    printf STDERR "-MGSCv37 == mm9\n";
    printf STDERR "-GRCm38 == mm10\n";
    printf STDERR "-Zv9 == danRer7\n";
    exit 255;
}

my $sequenceName = shift;
$sequenceName =~ s/^-//;

my $tmpDb = "tmpIncident${sequenceName}";

while (my $dir = shift) {
    my @issueId;  # index is autoSql assigned id, value is NCBI key2
    my %issues;   # key is key2, value is an issue structure with attributes
    my @summary;  # index is autoSql assigned, id, value is summary string
    my @resolutionText;  # index is autoSql assigned, id, value is resolutionText string
    my @resolution;  # index is autoSql assigned, id, value is resolution string
    my @description;  # index is autoSql assigned, id, value is description string
    my @mapSequence;  # index is autoSql assigned, id, value is mapSequence structure
    # read in some files that are just two columns, an id and a string
    # save in arrays
    my $file = "$dir/resolutionText.tab";
    readTagValue($file,\@resolutionText);
    $file = "$dir/resolution.tab";
    readTagValue($file,\@resolution);
    $file = "$dir/description.tab";
    readTagValue($file,\@description);
    $file = "$dir/summary.tab";
    readTagValue($file,\@summary);
    #	this file has three or four columns.  Save as structure in array
    $file = "$dir/mapSequence.tab";
    open (FH, "<$file") or die "can not read $file";
    while (my $line = <FH>) {
	chomp $line;
	my $id = 0;
	my $text = "";
	my $gbAcc = "";
	my $refAcc = "";
	my $type = "";
	my @a = split('\t', $line);
	if (scalar(@a) == 5) {
	    ($id, $text, $gbAcc, $refAcc, $type) = split('\t', $line);
	} elsif (scalar(@a) == 4) {
	    ($id, $text, $gbAcc, $refAcc) = split('\t', $line);
	} elsif (scalar(@a) == 3) {
	    ($text, $gbAcc, $refAcc) = split('\t', $line);
	} elsif (scalar(@a) < 3) {
	    next;
	} else {
	    printf STDERR "column count: %d in $file\n", scalar(@a);
	    printf STDERR "line: '%s'\n", $line;
	    die "can not recognize 3, 4 or 5 columns in $file";
	}
	if ($id eq "na") {
	    printf STDERR "WARN: discarding from $file: '$line' since id = 'na'\n";
	    next;
	}
	my %mapSequence;
	$mapSequence{'text'} = $text;
	$mapSequence{'gbAcc'} = $gbAcc;
	$mapSequence{'refAcc'} = $refAcc;
	$mapSequence{'type'} = $type;
	if (defined($mapSequence[$id])) {
	    die "duplicate mapSequence id: $id in $sequenceName";
	}
	$mapSequence[$id] = \%mapSequence;
    }
    close (FH);

    # index is location number, value is comma separated list of issue #'s
    my @locationToIssue;
    # index is location number, value is comma separated list of position #'s
    my @locationToPosition;
    # index is position number, value is the structure with the position
    # information
    my @position;
    # mouse and zebrafish have location index in the issue record
    # human has multiple locations in each issue and has
    #	an issueToLocation table
    $file = "$dir/issue.tab";
    printf STDERR "# reading $file\n" if ($debug);
    loadTmpTable($tmpDb, "issue", $file);
    open (FH, "/cluster/bin/x86_64/hgsql -N $tmpDb -e 'select id,type,key2,assignedChr,accession1,accession2,reportType,summary,status,statusText,description,experimentType,externalInfoType,upd,resolution,resolutionText,affectVersion,fixVersion,location from issue;'|")
        or die "can not select from $tmpDb.issue";
    while (my $line = <FH>) {
	chomp $line;
	my @a = split('\t', $line);
        my $fieldCount = scalar(@a);
        die "do not find 19 issue.tab, instead: $fieldCount in $sequenceName $file" if ( $fieldCount != 19 );
	my ($id, $type, $key2, $assignedChr, $accession1, $accession2,
	    $reportType, $summary, $status, $statusText, $description,
	    $experimentType, $externalInfoType, $upd, $resolution, 
	    $resolutionText, $affectVersion, $fixVersion,
            $location) = split('\t', $line);
	die "ERROR: id $id already seen in $file" if (exists($issueId[$id]));
	die "ERROR: key $key2 already exists" if (exists($issues{$key2}));
	my %issue;
	$issue{'type'} = $type;
	$issue{'assignedChr'} = $assignedChr;
	$issue{'status'} = $status;
	$issue{'reportType'} = $reportType;
	$issue{'summary'} = $summary;
	$issue{'description'} = $description;
	$issue{'resolution'} = $resolution;
	$issue{'resolutionText'} = $resolutionText;
	$issue{'issueId'} = $id;
die "location == 'na' in $file" if ($location eq "na");
	$issue{'location'} = $location;
	$issueId[$id] = $key2;
	$issues{$key2} = \%issue;
    }
    close (FH);

    # locationToPosition does not exist for zebrafish
    # and it isn't a unique key in column 1
    $file = "$dir/locationToPosition.tab";
    if (open (FH, "<$file")) {
        printf STDERR "# reading $file\n" if ($debug);
	while (my $line = <FH>) {
	    chomp $line;
	    my @a = split('\t', $line);
	    if (scalar(@a) != 2) {
		die "do not find 2 columns in issue.tab in $sequenceName";
	    }
	    my ($location, $position) = split('\t', $line);
	    # construct comma separated list of positions for a single location
	    if (defined($locationToPosition[$location])) {
		my $otherPositions = $locationToPosition[$location];
		$locationToPosition[$location] = sprintf("%s,%d", $otherPositions, $position);
	    } else {
		$locationToPosition[$location] = $position;
	    }
	}
	close (FH);
    }

    $file = "$dir/position.tab";
    open (FH, "<$file") or die "can not read $file";
    printf STDERR "# reading $file\n";
    my $positionIndex = 1;
    while (my $line = <FH>) {
	chomp $line;
	my @a = split('\t', $line);
	my $id = "na";
	my $name = "";
	my $gbAsmAcc = "";
	my $refAsmAcc = "";
	my $asmStatus = "";
	my $mapStatus = "";
	my $chr = "na";
	my $mapSequence = "na";
	my $start = 0;
	my $stop = 0;
	my $quality = "";
	if (scalar(@a) == 10) {
	    ($id, $name, $gbAsmAcc, $refAsmAcc, $asmStatus, $mapStatus,
		$mapSequence, $start, $stop, $quality) = split('\t', $line);
	} elsif (scalar(@a) == 9) {
	    ($name, $gbAsmAcc, $refAsmAcc, $asmStatus, $mapStatus,
		$mapSequence, $start, $stop, $quality) = split('\t', $line);
	} else {
	    die "did not find 9 or 10 columns in position.tab for $sequenceName";
	}
	my %position;
	$position{'id'} = $id;
	$position{'name'} = $name;
	$position{'gbAsmAcc'} = $gbAsmAcc;
	$position{'refAsmAcc'} = $refAsmAcc;
	$position{'asmStatus'} = $asmStatus;
	$position{'mapStatus'} = $mapStatus;
	$position{'mapSequence'} = $mapSequence;
	$position{'start'} = $start;
	$position{'stop'} = $stop;
	$position{'quality'} = $quality;
	if (defined($position[$positionIndex])) {
	    die "duplicated position $positionIndex in $sequenceName";
	} else {
	    $position[$positionIndex++] = \%position;
	}
    }
    close (FH);
    printf STDERR "read %d items from position.tab file\n", $positionIndex-1
	if ($debug);

    # now, with all data having been read in, peform the joins to get
    #	the final set of data
    foreach my $key (keys %issues) {
	my $issue = $issues{$key};
	my $location = $issue->{'location'};
	my $position = "na";
	# this location is either an index in locationToPosition, or
	# an index into position
	if (scalar(@locationToPosition) > 0) {
die "can not find location in locationToPosition[$location]" if (!defined($locationToPosition[$location]));
	    my $locationToPosition = $locationToPosition[$location];
	    my @positions = split(',', $locationToPosition);
	    for (my $i = 0; $i < scalar(@positions); ++$i) {
		my $findPosition = $position[$positions[$i]];
		if ($findPosition->{'name'} =~ m/$sequenceName/) {
		    $position = $findPosition;
		    last;
		}
	    }
	} else {
	    $position = $position[$location];
	}
	if ($position =~ m/na/) {
	    printf STDERR "WARN: can not find position for issue $key $sequenceName\n" if ($debug);
	    next;
	}
	my $mapChr = "na";
	# verify the mapSequence chromosome is the same as assignedChr
	my $mapSequence = $position->{'mapSequence'};
	# if it is simply a number, it is an index
	if ($mapSequence =~ m/^[0-9]+$/) {
	    # and their string is just a number for chrom name
	    my $mapPtr = $mapSequence[$mapSequence];
	    $mapChr = "chr" . $mapPtr->{'text'};
	}
	my $start = $position->{'start'};
	my $stop = $position->{'stop'};
	my $chr = $issue->{'assignedChr'};
	# the mapSequence chrom name can override the assignedChr
	if ($mapChr ne "na" && $chr ne $mapChr) {
	    printf STDERR "WARN: key: $key, resetting chr $chr to mapChr: $mapChr\n"
		if ($debug);
	    $chr = $mapChr;
	}
	if (($start eq "na") || ($stop eq "na")) {
	    printf STDERR "WARN: discarding $key, chrom: $chr since start($start) or stop($stop) is na\n" if ($debug);
	    next;
	}
	if ( ($chr =~ m/none/) || ($chr =~ m/\|/) || ($chr =~ m/un|na/i)) {
	    printf STDERR "WARN: discarding $key, since chrom($chr) is none or UN/NA\n" if ($debug);
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
	my $resolution = $issue->{'resolution'};
	if (scalar(@resolution) > 0 && $resolution =~ m/^[0-9]+$/) {
	    $resolution = $resolution[$resolution];
	}
	$resolution =~ s/\s+$//;
	my $resolutionText = $issue->{'resolutionText'};
	if (scalar(@resolutionText) > 0 && $resolutionText =~ m/^[0-9]+$/) {
	    $resolutionText = $resolutionText[$resolutionText];
	}
	$resolutionText =~ s/\s+$//;
	my $bestResolution = $resolution;
	if (length($bestResolution) < length($resolutionText)) {
	    $bestResolution = $resolutionText;
	}
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
	    $detailPage, $bestResolution);
	$detailPage =~ s/\s/&nbsp;/g;
	printf "%s\t%d\t%d\t%s\t%s\n", $chr, $start, $stop, $key,
		$detailPage;
    }
}
