#!/usr/bin/perl

use warnings;
use strict;
#use DataBrowser qw(browse);
use Getopt::Long;



#my $doencval = $ARGV[0];

#unless ($doencval) {
my $doencval = "../doEncodeValidate.pl";
#}

#unless ($doencval){
#	die "usage: tester.pl doEncodeValidate.pl\n";
#}

unless (-e $doencval) {
	die "can't find doEncodeValidate.pl in directory above, did you move the test directory?\n"
}


my $make;
if ($ARGV[1] && $ARGV[1] eq "make"){
	$make = 1
}
my %opt;

my $goodopts = GetOptions(\%opt,
					"verbose"
				);


my $tests = `find . -name "test.cfg"`;

my @tests = split "\n", $tests;

#my %tests;
my $verbose = $opt{'verbose'};



foreach my $test (@tests){
	my @errors;
	#print "test = $test\n";
	my $name;
	my $expected;
	open TEST, "$test";
	my $grab = 0;
	my @expectlines;
	my $testnum;
	my @header;
	while (<TEST>){
		my $line = $_;
	#	print "line = $line\n";
		chomp $line;
		if ($line =~ m/^\s*\#/){next}
		if ($line =~ m/^name (.*)$/){
			$name = $1;
			push @header, $line;
		}
	
		if ($line =~ m/^testnum (\d+)/){
			$testnum = $1;
			push @header, $line;
		}
		if ($line =~ m/^expected/){
			$grab = 1;
			push @header, $line;
			next;
		}
		if ($grab){
			push @expectlines, $line;
			
		}
	}	
	$test =~ m/^(.*)\/test.cfg/;
	my $folder = $1;
	close TEST;
	#print "folder = $folder\n";

	my $testresult = `perl $doencval -configDir config $testnum $folder 2>&1`;
	
	my @resultlines = split "\n", $testresult;
	$testresult = join "\n", @resultlines;
	if ($make) {
		push @header, $testresult;
	}
	
	my $count = 0;
	my $pass = 1;
	if (scalar(@resultlines) != scalar(@expectlines)){
		if ($verbose){
			my $count = 0;
			foreach my $line (@resultlines){
				unless($expectlines[$count]){
					$expectlines[$count] = 'NULL';
				}
				$count++;
			}
			push @errors, "line count of expected vs result don't match\n";
		}
		$pass = 0;
	}
	
	foreach my $compare (@expectlines){
		if (!defined($resultlines[$count])){
			$pass = 0;
			next
		}
		if ($compare =~ m/regex:(.*)$/){
			#print "here\n";
			my $compline = $1;
			unless ($resultlines[$count] =~ m/$compline/){
				if ($verbose){		
					print @errors, "expected = $compline\n";
					print @errors, "result = $resultlines[$count]\n";
				}
				$pass = 0;	
			}
		}
		elsif ($compare ne $resultlines[$count]) {
			
			$pass = 0;
			if ($verbose){
				push @errors, "expected = $compare\n";
				push @errors, "result = $resultlines[$count]\n";
			}
		}
		
		
		$count++
		
	}
	
	#print "testresult = $testresult\n";
	#if ($make){
	#	open TEST, ">$test";
	#	foreach my $line (@header){
	#		print TEST "$line\n";
	#	}
	
	#}
	
	if ($pass){
		print "Test: $name\n\tNo errors\n";
	}
	else {
		print "Test: $name\n\tTest Failed, you broke the code.\n";
		if (@errors && $verbose){
			foreach my $error (@errors){
				print "\t$error";
			}
		}

	}
	print "\n";

}

#browse(\%tests);



