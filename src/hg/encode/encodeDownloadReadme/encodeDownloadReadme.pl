#!/usr/bin/perl

use warnings;
use strict;
use Fcntl ':mode';
use Getopt::Long;


#option handling code

#required
my $assm;
#options
my %opt;
my $check = GetOptions(\%opt,
		"d=s",
		"c=s",
		"t"
		);
unless ($check){usage()}

unless ($ARGV[0]){
	usage()
}

$assm = $ARGV[0];
#preset directories
my $dldir = "/hive/groups/encode/dcc/analysis/ftp/pipeline/$assm/";
my $configdir = "/hive/groups/encode/dcc/pipeline/encpipeline_prod/config/";
my $readme = "README.txt";

#options switching
if ($opt{'d'}){
	$dldir = $opt{'d'};	
	print STDERR "using download directory: $dldir\n";
}
else {
	print STDERR "using download directory: $dldir\n";
}

if ($opt{'c'}){
	$configdir = $opt{'c'};
	print STDERR "using config directory: $configdir\n";
}
else {
	print STDERR "using config directory: $configdir\n";
}
if ($opt{'t'}){
	$readme = "README.txt.test";
}


my $readmeTemplate = "$configdir/downloadsReadmeTemplate.txt";

open TEMPLATE, "$readmeTemplate" or die "Can't open README template\n";

#grab template to be interpolated;
my @template;
while (<TEMPLATE>){

	my $line = $_;
	chomp $line;
	#skip commented lines
	if ($line =~ m/^\s*#/){next}
	push @template, $line;
	
}

close TEMPLATE;

#find only folders in $dldir that match wgEncode
my $folders = `find $dldir -maxdepth 1 -type d | grep "wgEncode"`;

my @list = split "\n", $folders;

#go through list of folders
foreach my $folder (@list){
	
	my $compname;
	
	#for some reason the search and replace below modifies the original array, so I copied it to prevent that
	my @localtemp = @template;
	#print "folder = $folder\n";	
	#grab composite name from directory
	if ($folder =~ m/\/(wgEncode.*)$/){
		
		$compname = $1;
		#print "compname = $compname\n";
	}
	else {next}
	my @stat = stat($folder);
	#print "$folder:\n";
	my $mode = $stat[2];
	my $writable= ($mode & S_IWGRP) >> 3;
	unless ($writable) {next}
	open README, ">$folder/$readme" or die "Can't open README file to write in directory $folder\n";

	foreach my $line (@localtemp){
		#interpolate in the name of the DB and composite name
		if ($line =~ m/\+\+/){
			$line =~ s/\+\+db\+\+/$assm/;
			$line =~ s/\+\+composite\+\+/$compname/;
			print README "$line\n";
		}
		else {
			print README "$line\n";
		}
	}
	close README;
}

print STDERR "\n\nPlease send out an e-mail reminder that the README.txt has been updated. We wouldn't want to have tracks in QA leaking out an older version.\n\n";



sub usage {
	
	print STDERR  <<END;
usage: encodeDownloadReadme.pl [assembly] [options]

Copies downloadReadmeTemplate.txt to all of the download directories with the proper link per composite 

Options:
	-d\tDownload directory location (default:/hive/groups/encode/dcc/analysis/ftp/pipeline/)
	-c\tConfig directory location (default:/hive/groups/encode/dcc/pipeline_prod/config/)
	-t\t(Boolean) Write README.txt.test instead of README.txt
	
END
exit 1;
}




__END__































