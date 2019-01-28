#!/usr/bin/env perl

use strict;
use warnings;
use HTTP::Tiny;
use Time::HiRes;
use JSON;

my $http = HTTP::Tiny->new();
my $server = 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi';
my $global_headers = { 'Content-Type' => 'application/json' };
my $last_request_time = Time::HiRes::time();
my $request_count = 0;

##############################################################################
###
### these functions were copied from Ensembl HTTP::Tiny example code:
###  https://github.com/Ensembl/ensembl-rest/wiki/Example-Perl-Client
###
##############################################################################

##############################################################################
sub perform_json_action {
  my ($endpoint, $parameters) = @_;
  my $headers = $global_headers;
  my $content = perform_rest_action($endpoint, $parameters, $headers);
  return {} unless $content;
  my $json = decode_json($content);
  return $json;
}

##############################################################################
sub perform_rest_action {
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
  }
  my $response = $http->get($url, {headers => $headers});
  my $status = $response->{status};
  if(!$response->{success}) {
    # Quickly check for rate limit exceeded & Retry-After (lowercase due to our client)
    if($status == 429 && exists $response->{headers}->{'retry-after'}) {
      my $retry = $response->{headers}->{'retry-after'};
      Time::HiRes::sleep($retry);
      # After sleeping see that we re-request
      return perform_rest_action($endpoint, $parameters, $headers);
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

#############################################################################
### main()
#############################################################################

my $json = JSON->new;
my $jsonReturn = perform_json_action("/list/publicHubs", "");
printf "%s", $json->pretty->encode( $jsonReturn );
