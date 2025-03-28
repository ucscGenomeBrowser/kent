#!/usr/bin/env perl

use strict;
use warnings;
use HTTP::Tiny;
use Time::HiRes;
use JSON;
use Getopt::Long;

# forward declaration
sub performRestAction($$$);

my $http = HTTP::Tiny->new();
# my $server = 'https://api.genome.ucsc.edu';
# my $server = 'https://apibeta.soe.ucsc.edu';
# api-test is actually hgwdev.gi.ucsc.edu == genome-test.gi.ucsc.edu
# using /usr/local/apache/cgi-bin/hubApi
# hgwdev-api is using /usr/local/apache/cgi-bin-api/hubApi
# my $server = 'https://hgwdev-api.gi.ucsc.edu';
my $globalHeaders = { 'Content-Type' => 'application/json' };
my $lastRequestTime = Time::HiRes::time();
my $processStartTime = Time::HiRes::time();
my $requestCount = 0;

##############################################################################
# command line options
my $server = 'https://api-test.gi.ucsc.edu'; # defaults to api-test but can be set via "binary" command line arg
my $endpoint = "";
my $hubUrl = "";
my $genome = "";
my $track = "";
my $chrom = "";
my $start = "";
my $end = "";
my $test0 = 0;
my $trackDump = 0;
my $debug = 0;
my $trackLeavesOnly = 0;
my $measureTiming = 0;
my $jsonOutputArrays = 0;
my $revComp = 0;
my $maxItemsOutput = "";
##############################################################################

sub usage() {
printf STDERR "usage: ./jsonConsumer.pl [arguments]\n";
printf STDERR "arguments:
-test0 - perform test of /list/publicHubs and /list/ucscGenomes endpoints
-trackDump - obtain all data for a single track from: track, genome (hubUrl)
           - proof of concept, will not work for all cases
-hubUrl=<URL> - use the URL to access the track or assembly hub
-genome=<name> - name for UCSC database genome or assembly/track hub genome
-track=<trackName> - specify a single track in a hub or database
-chrom=<chromName> - restrict the operation to a single chromosome
-start=<coordinate> - restrict the operation to a range, use both start and end
-end=<coordinate> - restrict the operation to a range, use both start and end
-maxItemsOutput=<N> - limit output to this number of items.  Default 1,000
                      maximum allowed 1,000,000
-trackLeavesOnly - for list tracks function, no containers listed
-measureTimeing - turn on timing measurement
-debug - turn on debugging business
-endpoint=<function> - where <function> is one of the following:
   /list/publicHubs - provide a listing of all available public hubs
   /list/ucscGenomes - provide a listing of all available UCSC genomes
   /list/hubGenomes - list genomes from a specified hub (with hubUrl=...)
   /list/tracks - list data tracks available in specified hub or database genome
   /list/chromosomes - list chromosomes from specified data track
   /list/schema - show schema from specified data track in hubUrl or database
   /getData/sequence - return sequence from specified hub or database genome
   /getData/track - return data from specified track in hub or database genome
";
}

#########################################################################
# generic output of a hash pointer
sub hashOutput($) {
  my ($hashRef) = @_;
  foreach my $key (sort keys %$hashRef) {
    my $value = $hashRef->{$key};
    $value = "<array>" if (ref($value) eq "ARRAY");
    $value = "<hash>" if (ref($value) eq "HASH");
     printf STDERR "%s - %s\n", $key, $hashRef->{$key};
  }
}

sub arrayOutput($) {
  my ($ary) = @_;
  my $i = 0;
  foreach my $element (@$ary) {
     printf STDERR "# %d\t%s\n", $i++, ref($element);
     if (ref($element) eq "HASH") {
       hashOutput($element);
     }
  }
}
#########################################################################

##############################################################################
###
### these functions were copied from Ensembl HTTP::Tiny example code:
###  https://github.com/Ensembl/ensembl-rest/wiki/Example-Perl-Client
###
##############################################################################

##############################################################################
sub performJsonAction($$) {
  my ($endpoint, $parameters) = @_;
  my $headers = $globalHeaders;
  my $content = performRestAction($endpoint, $parameters, $headers);
  return {} unless $content;
  my $json = decode_json($content);
  return $json;
}

##############################################################################
sub performRestAction($$$) {
  my ($endpoint, $parameters, $headers) = @_;
  $parameters ||= {};
  $headers ||= {};
  $headers->{'Content-Type'} = 'application/json' unless exists $headers->{'Content-Type'};
  if($requestCount == 15) { # check every 15
    my $currentTime = Time::HiRes::time();
    my $diff = $currentTime - $lastRequestTime;
    # if less than a second then sleep for the remainder of the second
    if($diff < 1) {
      Time::HiRes::sleep(1-$diff);
    }
    # reset
    $lastRequestTime = Time::HiRes::time();
    $requestCount = 0;
  }

  $endpoint =~ s#^/##;
  my $url = "$server/$endpoint";
  my $argSeparator = ";";

  if(%{$parameters}) {
    my @params;
    foreach my $key (sort {$a cmp $b} keys %{$parameters}) {
      my $value = $parameters->{$key};
      push(@params, "$key=$value");
    }
    my $param_string = join(';', @params);
    $url.= '?'.$param_string;
  } else {
    if ($debug || $measureTiming || $jsonOutputArrays || length($maxItemsOutput) ) {
     $argSeparator = "?";
   }
  }
  if ($debug) { $url .= "${argSeparator}debug=1"; }
  if ($measureTiming) { $url .= "${argSeparator}measureTiming=1"; }
  if ($jsonOutputArrays) { $url .= "${argSeparator}jsonOutputArrays=1"; }
  if ($revComp) { $url .= "${argSeparator}revComp=1"; }
  if (length($maxItemsOutput)) { $url .= "${argSeparator}maxItemsOutput=$maxItemsOutput"; }
  printf STDERR "### '%s'\n", $url;
  my $response = $http->get($url, {headers => $headers});
  my $status = $response->{status};
  if(!$response->{success}) {
    # Quickly check for rate limit exceeded & Retry-After (lowercase due to our client)
    if($status == 429 && exists $response->{headers}->{'retry-after'}) {
      my ($status, $reason) = ($response->{status}, $response->{reason});
      my $retry = $response->{headers}->{'retry-after'};
      printf STDERR "Failed for $endpoint! Status code: ${status}. Reason: ${reason}, retry-after: $retry seconds\n";
#      hashOutput($response->{headers});
      Time::HiRes::sleep($retry);
      # After sleeping see that we re-request
      return performRestAction($endpoint, $parameters, $headers);
    }
    else {
      my ($status, $reason) = ($response->{status}, $response->{reason});
#      die "Failed for $endpoint! Status code: ${status}. Reason: ${reason}\n";
      printf STDERR "Failed for $endpoint! Status code: ${status}. Reason: ${reason}\n";
# hashOutput($response->{headers});
# hashOutput($response->{content});
# printf STDERR "'%s'\n", $response->{content};
# printf STDERR "'%s'\n", $response->{headers};
      return return $response->{content};
    }
  }
  $requestCount++;
  if(length $response->{content}) {
    return $response->{content};
  }
  return;
}

#############################################################################
sub columnNames($) {
  my ($nameArray) = @_;
  if (ref($nameArray) ne "ARRAY") {
    printf "ERROR: do not have an array reference in columnNames\n";
  } else {
    printf "### Column names in table return:\n";
    my $i = 0;
    foreach my $name (@$nameArray) {
      printf "%d\t\"%s\"\n", ++$i, $name;
    }
  }
}

sub topLevelKeys($) {
  my ($topHash) = @_;
  printf "### keys in top level hash:\n";
  foreach my $topKey ( sort keys %$topHash) {
    # do not print out the downloadTime and downloadTimeStamps since that
    # would make it difficult to have a consistent test output.
    next if ($topKey eq "downloadTime");
    next if ($topKey eq "downloadTimeStamp");
    next if ($topKey eq "botDelay");
    next if ($topKey eq "dataTime");
    next if ($topKey eq "dataTimeStamp");
    my $value = $topHash->{$topKey};
    $value = "<array>" if (ref($value) eq "ARRAY");
    $value = "<hash>" if (ref($value) eq "HASH");
    printf "\"%s\":\"%s\"\n", $topKey,$value;
  }
}

#############################################################################
sub checkError($$$) {
  my ($json, $endpoint, $expect) = @_;
  my $jsonReturn = performJsonAction($endpoint, "");
#   printf "%s", $json->canonical()->pretty->encode( $jsonReturn );
  if (! defined($jsonReturn->{'error'}) ) {
     printf "ERROR: no error received from endpoint: '%s', received:\n", $endpoint;
     printf "%s", $json->canonical()->pretty->encode( $jsonReturn );
  } else {
     if ($jsonReturn->{'error'} ne "$expect '$endpoint'") {
	printf "incorrect error received from endpoint '%s':\n\t'%s'\n", $endpoint, $jsonReturn->{'error'};
     }
     printf "%s", $json->canonical()->pretty->encode( $jsonReturn );
  }
}

#############################################################################
sub verifyCommandProcessing()
{
    my $json = JSON->new;
    # verify command processing can detected bad input
    my $endpoint = "/list/noSubCommand";
    my $expect = "do not recognize endpoint function:";
    checkError($json, $endpoint,$expect);
}	#	sub verifyCommandProcessing()

#############################################################################
#  Find the highest chromStart in the returned to data to obtain a continuation
#  point.
#  The item 'chromStart' is not necessarily always named as such,
#    depending upon track type, it could be: tStart or genoStart or txStart
sub findHighestChromStart($$) {
  my $highStart = -1;
  my ($hashPtr, $track) = @_;
  my $trackData = $hashPtr->{$track};
  foreach my $item (@$trackData) {
    if (defined($item->{'tStart'})) {
       $highStart = $item->{'tStart'} if ($item->{'tStart'} > $highStart);
    } elsif (defined($item->{'genoStart'})) {
       $highStart = $item->{'genoStart'} if ($item->{'genoStart'} > $highStart);
    } elsif (defined($item->{'txStart'})) {
       $highStart = $item->{'txStart'} if ($item->{'txStart'} > $highStart);
    } elsif (defined($item->{'chromStart'})) {
     $highStart = $item->{'chromStart'} if ($item->{'chromStart'} > $highStart);
    } else {
       die "ERROR: do not recognize table type for track '%s', can not find chrom start.\n", $track;
    }
  }
  return $highStart;
}

#############################################################################
# walk through all the chromosomes for a track to extract all data
# XXX - NOT ADDRESSED - this produces duplicate items at the breaks when
#       maxItemsLimit is used
sub trackDump($$) {
  my ($endpoint, $parameters) = @_;
  my $errReturn = 0;
  my %localParameters;
  if (length($hubUrl)) {
     $localParameters{"hubUrl"} = "$hubUrl";
  }
  if (length($genome)) {
     $localParameters{"genome"} = "$genome";
  }
  if (length($track)) {
     $localParameters{"track"} = "$track";
  }
  my $endPoint = "/list/chromosomes";
  my $jsonChromosomes = performJsonAction($endPoint, \%localParameters);
  $errReturn = 1 if (defined ($jsonChromosomes->{'error'}));
  my $json = JSON->new;
  my %chromInfo;	# key is chrom name, value is size
  if (0 == $errReturn) {
    my $chromHash = $jsonChromosomes->{'chromosomes'};
    foreach my $chr (keys %$chromHash) {
      $chromInfo{$chr} = $chromHash->{$chr};
    }
    # for each chromosome, in order by size, smallest first
    $endPoint = "/getData/track";
    $maxItemsOutput = 14000;
    foreach my $chr (sort {$chromInfo{$a} <=> $chromInfo{$b}} keys %chromInfo) {
      $localParameters{"chrom"} = "$chr";
      delete $localParameters{'start'};
      delete $localParameters{'end'};
      printf STDERR "# working\t%s\t%d\n", $chr, $chromInfo{$chr};
      my $oneChrom = performJsonAction($endPoint, \%localParameters);
      my $itemsReturned = $oneChrom->{'itemsReturned'};
      my $reachedMaxItems = 0;
      $reachedMaxItems = 1 if (defined($oneChrom->{'maxItemsLimit'}));
      if ($reachedMaxItems) {
         while ($reachedMaxItems) {
           my $highestChromStart = findHighestChromStart($oneChrom, $track);
           printf STDERR "# chrom: %s\t%d items -> max item limit last chromStart %d\n", $chr, $itemsReturned, $highestChromStart;
	   $localParameters{'start'} = "$highestChromStart";
	   $localParameters{'end'} = "$chromInfo{$chr}";
           $reachedMaxItems = 0;
           $oneChrom = performJsonAction($endPoint, \%localParameters);
           $itemsReturned = $oneChrom->{'itemsReturned'};
           $reachedMaxItems = 1 if (defined($oneChrom->{'maxItemsLimit'}));
           if (0 == $reachedMaxItems) {
             $highestChromStart = findHighestChromStart($oneChrom, $track);
             printf STDERR "# chrom: %s\t%d items completed at last chromStart %d\n", $chr, $itemsReturned, $highestChromStart;
           }
         }
      } else {
         printf STDERR "# chrom: %s\t%d items - completed\n", $chr, $itemsReturned;
      }
    }	# foreach chrom in chromInfo
  }	# if (0 == $errReturn)  chromInfo was successful

  return $errReturn;
}	#	sub trackDump($$)

#############################################################################
sub processEndPoint() {
  my $errReturn = 0;
  if (length($endpoint)) {
     my $json = JSON->new;
     my $jsonReturn = {};
     my %parameters;
     if (length($hubUrl)) {
	$parameters{"hubUrl"} = "$hubUrl";
     }
     if (length($genome)) {
	$parameters{"genome"} = "$genome";
        }
     if (length($chrom)) {
	$parameters{"chrom"} = "$chrom";
     }
     if ($trackLeavesOnly) {
	$parameters{"trackLeavesOnly"} = "1";
     }
     if (length($track)) {
	$parameters{"track"} = "$track";
     }
     if (length($start)) {
	$parameters{"start"} = "$start";
     }
     if (length($end)) {
	$parameters{"end"} = "$end";
     }
     #	Pass along any bogus request just to test the error handling.
     if ($trackDump) {
        $errReturn = trackDump($endpoint, \%parameters);
     } else {
        $jsonReturn = performJsonAction($endpoint, \%parameters);
        $errReturn = 1 if (defined ($jsonReturn->{'error'}));
        printf "%s", $json->canonical()->pretty->encode( $jsonReturn );
     }
  } else {
    printf STDERR "ERROR: no endpoint given ?\n";
    usage();
    exit 255;
  }
  return $errReturn;
}	# sub processEndPoint()

###########################################################################
### test /list/publicHubs and /list/ucscGenomes
sub test0() {

my $json = JSON->new;
my $jsonReturn = {};

# verify command processing can detected bad input
# /list/noSubCommand
verifyCommandProcessing();	# check 'command' and 'subCommand'

$jsonReturn = performJsonAction("/list/publicHubs", "");

# this prints everything out indented nicely:
# printf "%s", $json->canonical()->pretty->encode( $jsonReturn );

# exit 255;
# __END__

#	"dataTimeStamp" : 1552320994,
#	"downloadTime" : "2019:03:26T21:40:10Z",
#	"botDelay" : 2,
#	"downloadTimeStamp" : 1553636410,
#	"dataTime" : "2019-03-11T09:16:34"

# look for the specific public hub named "Plants" to print out
# for a verify test case
#
if (ref($jsonReturn) eq "HASH") {
  topLevelKeys($jsonReturn);

  if (defined($jsonReturn->{"publicHubs"})) {
     my $arrayData = $jsonReturn->{"publicHubs"};
     foreach my $data (@$arrayData) {
	if ($data->{'shortLabel'} eq "Synonymous Constraint") {
        printf "### Synonymous Constraint public hub data\n";
	  foreach my $key (sort keys %$data) {
	  next if ($key eq "registrationTime");
	  printf "'%s'\t'%s'\n", $key, $data->{$key};
	  }
	}
     }
  }
} elsif (ref($jsonReturn) eq "ARRAY") {
  printf "ERROR: top level returns ARRAY of size: %d\n", scalar(@$jsonReturn);
  printf "should have been a HASH to the publicHub data\n";
}

$jsonReturn = performJsonAction("/list/ucscGenomes", "");
# printf "%s", $json->pretty->encode( $jsonReturn );


if (ref($jsonReturn) eq "HASH") {
  topLevelKeys($jsonReturn);
  if (defined($jsonReturn->{"ucscGenomes"})) {
     my $ucscGenomes = $jsonReturn->{"ucscGenomes"};
     if (exists($ucscGenomes->{'hg38'})) {
	my $hg38 = $ucscGenomes->{'hg38'};
        printf "### hg38/Human information\n";
     foreach my $key (sort keys %$hg38) {
	   printf "\"%s\"\t\"%s\"\n", $key, $hg38->{$key};
         }
       }
     }
} elsif (ref($jsonReturn) eq "ARRAY") {
  printf "ERROR: top level returns ARRAY of size: %d\n", scalar(@$jsonReturn);
  printf "should have been a HASH to the ucscGenomes\n";
}

}	#	sub test0()

sub elapsedTime() {
if ($measureTiming) {
  my $endTime = Time::HiRes::time();
  my $et = $endTime - $processStartTime;
  printf STDERR "# procesing time: %.3fs\n", $et;
}
}

#############################################################################
### main()
#############################################################################

my $argc = scalar(@ARGV);

GetOptions ("hubUrl=s" => \$hubUrl,
    "endpoint=s"  => \$endpoint,
    "genome=s"  => \$genome,
    "track=s"  => \$track,
    "chrom=s"  => \$chrom,
    "start=s"  => \$start,
    "end=s"    => \$end,
    "test0"    => \$test0,
    "trackDump"    => \$trackDump,
    "debug"    => \$debug,
    "trackLeavesOnly"    => \$trackLeavesOnly,
    "measureTiming"    => \$measureTiming,
    "jsonOutputArrays"    => \$jsonOutputArrays,
    "revComp=s"    => \$revComp,
    "maxItemsOutput=s"   => \$maxItemsOutput,
    "serverName=s"           => \$server)
    or die "Error in command line arguments\n";

if ($test0) {
   test0;
   elapsedTime();
   exit 0;
}

if ($argc > 0) {
   if (processEndPoint()) {
	elapsedTime();
	exit 255;
   } else {
	elapsedTime();
	exit 0;
   }
}

usage();
