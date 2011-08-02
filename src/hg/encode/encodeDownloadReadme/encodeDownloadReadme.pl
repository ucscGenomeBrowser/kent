#!/usr/bin/perl

use warnings;
use strict;
use Getopt::Std;


#option handling code

#required
my $assm;

unless($ARGV[0]){
	usage();
}
elsif ($ARGV[0] =~ m/^\-/) {
	usage();
}
else {
	$assm = shift (@ARGV);
}

if ($ARGV[0]){
	unless ($ARGV[0] =~ m/^\-/){
		usage();
	}
}

#options
my %opt;
getopts("d:c:t", \%opt);

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
	
	#grab composite name from directory
	if ($folder =~ m/\/(wgEncode.*)$/){
		$compname = $1;
	}

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





sub usage {
	print STDERR "\n\nusage: encodeDownloadReadme.pl [assembly] [options]\n";
	print STDERR "\tOptions:\n";
	print STDERR "\t-d\tDownload directory location (default:/hive/groups/encode/dcc/analysis/ftp/pipeline/)\n";
	print STDERR "\t-c\tConfig directory location (default:/hive/groups/encode/dcc/pipeline/config/)\n";
	print STDERR "\t-t\t(Boolean) Write README.txt.test instead of README.txt\n";
	die "\n";

}




__END__































