#!/usr/bin/env perl

# $Id: scanSettings.pl,v 1.3 2009/11/19 23:35:31 hiram Exp $

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc < 1) {
    printf STDERR "scanSettings.pl - scan output of hgTrackDb -settings,
    implement ClosestToHome API to indicate applicable settings for each table.

usage: scanSettings.pl [options] hg18.settings.txt
options:
  -type=trackType - show only tracks of 'trackType'
	where trackType is the first word of type line
  -noParents - do not show parent composite container tracks
  -show=comma,separated,list - of settings to display

  where hg18.settings.txt is from this in trackDb directory:
  make EXTRA='-strict -settings' DBS=hg18 > hg18.settings.txt 2>&1
The input file can also be 'stdin', thus:
  make EXTRA='-strict -settings' DBS=hg18 2>&1 \\
     | ../../utils/scanSettings.pl stdin\n";
    exit 255;
}

my $noParents = 0;	# 1 indicates do not show parents
my $trackType = "";	# first word of track type line
my %showList;		# key is setting name to show
my $listOnly = 0;	# 1 means to check the showList

my %tableList;	# key is table name, value is hash of settings
my $file = "noFileSpecified";
while (my $arg = shift) {
    if ($arg =~ m/-noParents/) {
	$noParents = 1;
	printf STDERR "# not showing composite parents\n";
    } elsif ($arg =~ m/-type/) {
	(my $dummy, $trackType) = split('=', $arg);
	printf STDERR "# showing only track type '$trackType'\n";
    } elsif ($arg =~ m/-show/) {
	(my $dummy, my $list) = split('=', $arg);
	my @show = split(',', $list);
	for (my $i = 0; $i < scalar(@show); ++$i) {
	    $showList{$show[$i]} = 1;
	}
	$listOnly = 1 if (scalar(@show) > 0);
    } else {
	$file = $arg;
    }
}

if ($file =~ m/stdin/) {
    open (FH, "</dev/stdin") or die "can not read stdin";
} else {
    open (FH, "<$file") or die "can not read $file";
}
while (my $line = <FH>) {
    next if ($line =~ m/^#/);
    next if ($line =~ m/^Warning: /);
    next if ($line =~ m/^Loaded /);
    next if ($line =~ m/^Sorted /);
    next if ($line =~ m/^hgTrackDb /);
    next if ($line =~ m/^hgFindSpec /);
    next if ($line =~ m#^./loadTracks #);
    chomp $line;
    my ($tableName, $rest) = split(': ', $line, 2);
    my @a = split('; ', $rest);
    die "no type= for $tableName" if ($a[0] !~ m/^type='/);
    my $fieldCount = scalar(@a);
    die "no fields for $tableName" if ($fieldCount < 1);
    $a[0] =~ s/^type='//;
    $a[0] =~ s/';$//;
    $a[0] =~ s/'$//;
    $a[$fieldCount-1] =~ s/;\s*$//;
    my %settingsHash;
    die "duplicate table name $tableName ?" if (exists($tableList{$tableName}));
    $tableList{$tableName} = \%settingsHash;
    $settingsHash{'type'} = $a[0];
    my $minLimit = "";
    my $maxLimit = "";
    my $viewLimits = "";
    for (my $i = 1; $i < $fieldCount; ++$i) {
	my ($name, $setting) = split('\s', $a[$i], 2);
	$settingsHash{$name} = $setting;
    }
#    printf "%s: '%s'\n", $tableName, $a[$fieldCount-1];
#    printf "%s - %s %s %s %s\n", $tableName, $a[0], $minLimit,
#	$maxLimit, $viewLimits;
}
close (FH);

# scan all composite tracks and pick up all view settings from them
my %viewSettings;  # key is table.view value are settings for that view
foreach my $table (keys %tableList) {
    my $hashRef = $tableList{$table};
    if (exists($hashRef->{'compositeTrack'})) {
	next if (exists($hashRef->{'superTrack'}));
	die "compositeTrack not 'on' ? $hashRef->{'compositeTrack'}"
		if ($hashRef->{'compositeTrack'} !~ m/^on$/);
	foreach my $setting (keys %$hashRef) {
	    if ($setting =~ m/settingsByView/) {
		my @views = split('\s+', $hashRef->{$setting});
		for (my $j = 0; $j < scalar(@views); ++$j) {
		    my ($view, $values) = split(':', $views[$j], 2);
		    my $viewKey = "$table.$view";
		    my @viewSettings = split(',', $values);
		    my %viewHash;
		    die "duplicate settings view $viewKey"
			if (exists($viewSettings{$viewKey}));
		    $viewSettings{$viewKey} = \%viewHash;
		    for (my $k = 0; $k < scalar(@viewSettings); ++$k) {
			my ($name, $value) = split('=', $viewSettings[$k], 2);
#			$viewHash{$name} = $value;
			$viewHash{$name} = $viewSettings[$k];
		    }
		}
#		printf "%s: %s='%s'\n", $table, $setting, $hashRef->{$setting};
	    }
	}
    }
}

# scan all sub tracks and add view settings or parent settings
#	when OK to do that.  This implements the ClosestToHome API
foreach my $table (keys %tableList) {
    my $hashRef = $tableList{$table};
    if (exists($hashRef->{'subTrack'})) {
	my $parent = $hashRef->{'subTrack'};
	$parent =~ s/\s+.*//;
	# first priority are subGroup viewSettings coming in from the parent
	if (exists($hashRef->{'subGroups'})) {
	    my @b = split('\s+', $hashRef->{'subGroups'});
	    for (my $m = 0; $m < scalar(@b); ++$m) {
		if ($b[$m] =~ m/^view=/) {
		    my ($dummy, $view) = split('=', $b[$m], 2);
		    my $viewKey = "$parent.$view";
		    my $viewHash = $viewSettings{$viewKey};
		    foreach my $viewKey (keys %$viewHash) {
			next if (exists($hashRef->{$viewKey}));
			# OK to inherit this setting
# printf "inheriting from $parent view $view to $table '$viewHash->{$viewKey}'\n";
			$hashRef->{$viewKey} = $viewHash->{$viewKey};
		    }
		}
	    }
	}
	#	next priority are parent composite settings when inherit OK
	my $parentSettings = $tableList{$parent};
	if ( (!exists($hashRef->{'noInherit'}) ||
		$hashRef->{'noInherit'} !~ m/^on$/) &&
	    (!exists($parentSettings->{'noInherit'}) ||
		$parentSettings->{'noInherit'} !~ m/^on$/) ) {
	    foreach my $parentKey (keys %$parentSettings) {
		next if ($parentKey =~ m/^track$/);
		next if ($parentKey =~ m/^compositeTrack$/);
		next if ($parentKey =~ m/^subGroup[0-9]*$/);
		next if ($parentKey =~ m/^dimensions$/);
		next if ($parentKey =~ m/^sortOrder$/);
		next if ($parentKey =~ m/^dragAndDrop$/);
		next if ($parentKey =~ m/^noInherit$/);
		next if ($parentKey =~ m/^visibilityViewDefaults$/);
		next if ($parentKey =~ m/^settingsByView$/);
		next if (exists $hashRef->{$parentKey} );
		next if (!defined $parentSettings->{$parentKey} );
		# OK to inherit this setting
		my $parentSetting = $parentSettings->{$parentKey};
# printf "inheriting from $parent to $table $parentKey='$parentSetting'\n";
		$hashRef->{$parentKey} = $parentSetting;
	    }
	}
    }
}

# ready to print out everything
foreach my $table (keys %tableList) {
    my $hashRef = $tableList{$table};
    my $type = $hashRef->{'type'};
    printf STDERR "no track type for '$table'\n" if (length($type) < 1);
    next if (length($type) < 1);
    my @typeWords = split('\s+', $type);
    next if ($noParents == 1 && exists($hashRef->{'compositeTrack'}));
    next if ( (length($trackType) > 0) && ($typeWords[0] ne $trackType) );
#	printf "%s: type='%s'", $table, $hashRef->{'type'};
    printf "%s:", $table;
    foreach my $setting (sort keys %$hashRef) {
	if ($listOnly) {
	    printf " %s='%s'", $setting, $hashRef->{$setting}
		if exists($showList{$setting});
	} else {
	    printf " %s='%s'", $setting, $hashRef->{$setting};
	}
	if ($setting =~ m/^view$/) {
	    my $viewKey = "$table.$hashRef->{$setting}";
	    die "can not find viewKey $viewKey"
		if (!exists($viewSettings{$viewKey}));
	    my $viewHash = $viewSettings{$viewKey};
	    foreach my $viewName (keys %$viewHash) {
		if ($listOnly) {
		    printf " %s='%s'", $viewName, $viewHash->{$viewName}
			if exists($showList{$viewName});
		} else {
		    printf " %s='%s'", $viewName, $viewHash->{$viewName};
		}
	    }
    }
    }
    printf "\n";
}
