#!/usr/bin/env perl

use strict;
use warnings;
use HTTP::Tiny;
use Time::HiRes;
use JSON;
use Getopt::Long;

my $http = HTTP::Tiny->new();
my $server = 'https://hgwdev-api.gi.ucsc.edu';
my $global_headers = { 'Content-Type' => 'application/json' };
my $last_request_time = Time::HiRes::time();
my $request_count = 0;

##############################################################################
# command line options
my $endpoint = "";
my $hubUrl = "";
my $genome = "";
my $db = "";
my $track = "";
my $chrom = "";
my $start = "";
my $end = "";
my $maxItemsOutput = "";
##############################################################################

##############################################################################
###
### these functions were copied from Ensembl HTTP::Tiny example code:
###  https://github.com/Ensembl/ensembl-rest/wiki/Example-Perl-Client
###
##############################################################################

##############################################################################
sub performJsonAction {
  my ($endpoint, $parameters) = @_;
  my $headers = $global_headers;
  my $content = performRestAction($endpoint, $parameters, $headers);
  return {} unless $content;
  my $json = decode_json($content);
  return $json;
}

##############################################################################
sub performRestAction {
  my ($endpoint, $parameters, $headers) = @_;
  $parameters ||= {};
  $headers ||= {};
  $headers->{'Content-Type'} = 'application/json' unless exists $headers->{'Content-Type'};
  if($request_count == 15) { # check every 15
    my $current_time = Time::HiRes::time();
    my $diff = $current_time - $last_request_time;
    # if less than a second then sleep for the remainder of the second
    if($diff < 1) {
      Time::HiRes::sleep(1-$diff);
    }
    # reset
    $last_request_time = Time::HiRes::time();
    $request_count = 0;
  }

  my $url = "$server/$endpoint";

  if(%{$parameters}) {
    my @params;
    foreach my $key (keys %{$parameters}) {
      my $value = $parameters->{$key};
      push(@params, "$key=$value");
    }
    my $param_string = join(';', @params);
    $url.= '?'.$param_string;
#     printf STDERR "# url: '%s'\n", $url;
  }
  my $response = $http->get($url, {headers => $headers});
  my $status = $response->{status};
  if(!$response->{success}) {
    # Quickly check for rate limit exceeded & Retry-After (lowercase due to our client)
    if($status == 429 && exists $response->{headers}->{'retry-after'}) {
      my $retry = $response->{headers}->{'retry-after'};
      Time::HiRes::sleep($retry);
      # After sleeping see that we re-request
      return performRestAction($endpoint, $parameters, $headers);
    }
    else {
      my ($status, $reason) = ($response->{status}, $response->{reason});
      die "Failed for $endpoint! Status code: ${status}. Reason: ${reason}\n";
    }
  }
  $request_count++;
  if(length $response->{content}) {
    return $response->{content};
  }
  return;
}

# generic output of a has pointer
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
#   printf "%s", $json->pretty->encode( $jsonReturn );
  if (! defined($jsonReturn->{'error'}) ) {
     printf "ERROR: no error received from endpoint: '%s', received:\n", $endpoint;
     printf "%s", $json->pretty->encode( $jsonReturn );
  } else {
     if ($jsonReturn->{'error'} ne "$expect '$endpoint'") {
	printf "incorrect error received from endpoint '%s':\n\t'%s'\n", $endpoint, $jsonReturn->{'error'};
     }
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
sub processEndPoint() {
  if (length($endpoint) > 0) {
     my $json = JSON->new;
     my $jsonReturn = {};
     if ($endpoint eq "/list/hubGenomes") {
        if (length($hubUrl) > 0) {
	   my %parameters;
	   $parameters{"hubUrl"} = "$hubUrl";
	   $jsonReturn = performJsonAction($endpoint, \%parameters);
	   printf "%s", $json->pretty->encode( $jsonReturn );
        } else {
	  printf STDERR "ERROR: need to specify a hubUrl for endpoint '%s'\n", $endpoint;
	  exit 255;
        }
     } elsif ($endpoint eq "/list/tracks") {
	my $failing = 0;
	my %parameters;
	if (length($db) > 0) {
	    $parameters{"db"} = "$db";
	} elsif (length($hubUrl) < 1) {
          printf STDERR "ERROR: need to specify a hubUrl for endpoint '%s'\n", $endpoint;
	  ++$failing;
	} else {
	  $parameters{"hubUrl"} = "$hubUrl";
	  if (length($genome) < 1) {
            printf STDERR "ERROR: need to specify a genome for endpoint '%s'\n", $endpoint;
	    ++$failing;
	  } else {
	    $parameters{"genome"} = "$genome";
	  }
	}
	if ($failing) { exit 255; }
	$jsonReturn = performJsonAction($endpoint, \%parameters);
	printf "%s", $json->pretty->encode( $jsonReturn );
     } elsif ($endpoint eq "/list/chromosomes") {
	my $failing = 0;
	my %parameters;
	if (length($db) > 0) {
	    $parameters{"db"} = "$db";
	} else {
          printf STDERR "ERROR: need to specify a db for endpoint '%s'\n", $endpoint;
	}
	if ($failing) { exit 255; }
	$jsonReturn = performJsonAction($endpoint, \%parameters);
	printf "%s", $json->pretty->encode( $jsonReturn );
     } elsif ($endpoint eq "/getData/sequence") {
	my $failing = 0;
	my %parameters;
	if (length($db) > 0) {
	    $parameters{"db"} = "$db";
	} elsif (length($hubUrl) < 1) {
          printf STDERR "ERROR: need to specify a hubUrl for endpoint '%s'\n", $endpoint;
	  ++$failing;
	} else {
	  $parameters{"hubUrl"} = "$hubUrl";
	  if (length($genome) < 1) {
            printf STDERR "ERROR: need to specify a genome for endpoint '%s'\n", $endpoint;
	    ++$failing;
	  } else {
	    $parameters{"genome"} = "$genome";
	  }
	}
	if (length($chrom) > 0) {
	    $parameters{"chrom"} = "$chrom";
	}
	if (length($start) > 0) {
	    $parameters{"start"} = "$start";
	    $parameters{"end"} = "$end";
	}
	if ($failing) { exit 255; }
	$jsonReturn = performJsonAction($endpoint, \%parameters);
	printf "%s", $json->pretty->encode( $jsonReturn );
     } elsif ($endpoint eq "/getData/track") {
	my $failing = 0;
	my %parameters;
	if (length($db) > 0) {
	    $parameters{"db"} = "$db";
	} elsif (length($hubUrl) < 1) {
          printf STDERR "ERROR: need to specify a hubUrl for endpoint '%s'\n", $endpoint;
	  ++$failing;
	} else {
	  $parameters{"hubUrl"} = "$hubUrl";
	  if (length($genome) < 1) {
            printf STDERR "ERROR: need to specify a genome for endpoint '%s'\n", $endpoint;
	    ++$failing;
	  } else {
	    $parameters{"genome"} = "$genome";
	  }
	}
	if (length($track) > 0) {
	    $parameters{"track"} = "$track";
	}
	if (length($chrom) > 0) {
	    $parameters{"chrom"} = "$chrom";
	}
	if (length($start) > 0) {
	    $parameters{"start"} = "$start";
	    $parameters{"end"} = "$end";
	}
	if ($failing) { exit 255; }
	$jsonReturn = performJsonAction($endpoint, \%parameters);
	printf "%s", $json->pretty->encode( $jsonReturn );
     } else {
	printf STDERR "# TBD: '%s'\n", $endpoint;
     }
  } else {
    printf STDERR "ERROR: no endpoint given ?\n";
    exit 255;
  }
}	# sub processEndPoint()

#############################################################################
### main()
#############################################################################

my $argc = scalar(@ARGV); 

GetOptions ("hubUrl=s" => \$hubUrl,
    "endpoint=s"  => \$endpoint,
    "genome=s"  => \$genome,
    "db=s"  => \$db,
    "track=s"  => \$track,
    "chrom=s"  => \$chrom,
    "start=s"  => \$start,
    "end=s"    => \$end,
    "maxItemsOutput=s"   => \$maxItemsOutput)
    or die "Error in command line arguments\n";

if ($argc > 0) {
   processEndPoint();
   exit 0;
}

my $json = JSON->new;
my $jsonReturn = {};

verifyCommandProcessing();	# check 'command' and 'subCommand'

$jsonReturn = performJsonAction("/list/publicHubs", "");

# this prints everything out indented nicely:
# printf "%s", $json->pretty->encode( $jsonReturn );

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
	if ($data->{'shortLabel'} eq "Plants") {
        printf "### Plants public hub data\n";
	  foreach my $key (sort keys %$data) {
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

# "ucscGenomes" : {
#       "ochPri3" : {


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

exit 255;
# the return is a hash with one key 'publicHubs',
#   the hash element for that key is an array:
my $resultArray = $jsonReturn->{"publicHubs"};
# each array element is a hash of the tag/value pairs for each track hub
# print out two of the tag/value pairs for each track hub
my $i = 0;
foreach my $arrayElement (@$resultArray) {
  printf STDERR "# %d - '%s' - '%s'\n", ++$i, $arrayElement->{"shortLabel"},
     $arrayElement->{"dbList"};
}

exit 0;

#  to investigate the complete structure, walk through it and
#  decide on what to do depending upon what type of elements are
#  discovered
if (ref($jsonReturn) eq "HASH") {
   printf STDERR "# jsonReturn is HASH\n";
   for my $key (sort keys %$jsonReturn) {
       my $element = $jsonReturn->{$key};
       printf STDERR "# type of $key element is: '%s'\n", ref($element);
       if (ref($element) eq "ARRAY") {
	  arrayOutput($element);
       } else {
  printf STDERR "# puzzling, expecting the single element in the HASH to be an ARRAY ?\n";
  printf STDERR "# instead it reports ref(\$jsonReturn->{'publicHubs') is: '%s'\n", ref($jsonReturn->{'publicHubs'});
       }
   }
} else {
  printf STDERR "# puzzling, expecting the jsonReturn to be a HASH ?\n";
  printf STDERR "# instead it reports ref(\$jsonReturn) is: '%s'\n", ref($jsonReturn);
}

__END__
# 0     HASH
dbCount - 1
dbList - hg19,
descriptionUrl -
hubUrl - http://web.stanford.edu/~htilgner/2012_454paper/data/hub.txt
longLabel - Whole-Cell 454 Hela and K562 RNAseq
registrationTime - 2013-09-26 16:05:56
shortLabel - 454 K562andHelaS3RNAseq
# 1     HASH

