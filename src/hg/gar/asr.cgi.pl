#!/usr/bin/perl
##
##  asr -- Assembly Request - receive assembly build requests
##

use CGI;
use URI::Escape;
use HTML::Entities;

use strict;
use warnings;

my $httpRefer = "noReference";
my $referDomain = "noDomain";
my $domainMustBe = "ucsc.edu";
my $httpReferMustBe = "assemblySearch.html";
# obscure the email address from source tree robot scanning
my $myAddr = 'hclawson at ucsc dot edu';
$myAddr =~ s/ at /@/;
$myAddr =~ s/ dot /./;
my $legitimateFrom = $myAddr;
my $sendTo = $myAddr;
my $Cc = $myAddr;
my $bounceAddr = $myAddr;
my $genArkRequestGroup = 'genark-request-group at ucsc dot edu';
$genArkRequestGroup =~ s/ at /@/;
$genArkRequestGroup =~ s/ dot /./;
# DBG - uncomment this to prevent email to genArkRequestGroup
# $genArkRequestGroup = $myAddr;

my $query = CGI->new;

my $referer = "noReferer";

if (exists($ENV{'HTTP_REFERER'}) && defined($ENV{'HTTP_REFERER'})) {
  $referer = CGI->escapeHTML($ENV{'HTTP_REFERER'});
  my @a = split('/', $referer);
  $httpRefer = $a[-1];  # should be "assemblyRequest.html"
  $httpRefer =~ s/\?.*//;	# remove arguments if present
  my @b = split('\.', $a[-2]);
  $referDomain = "$b[-2].$b[-1]";	# should be "ucsc.edu"
}

print $query->header(-type => 'text/html');

print "<html><head><title>Assembly Search Request assembly build</title></head>\n";
print "<body>\n";

# QUERY_STRING    name=some%20name&email=some@email.com&asmId=GCF_000951035.1_Cang.pa_1.0

my %incoming = (
  "name" => "noName",
  "email" => "noEmail",
  "asmId" => "noAsmId",
  "betterName" => "noBetterName",
  "comment" => "noComment",
);

my $validIncoming = 0;
my $extraneousArgs = 0;

# limit all to reasonable lengths
my $maxLength = 1024;

foreach my $tag ($query->param) {
  my $value = $query->escapeHTML(uri_unescape($query->param($tag)));
  # only accept known inputs, the five defined above for %incoming defaults
  if (defined($incoming{$tag}) && defined($value)) {
      $incoming{$tag} = substr($value, 0, $maxLength);
      ++$validIncoming;
  } else {
    ++$extraneousArgs;
  }
}

if ( ($validIncoming != 5) || ($extraneousArgs > 0) || ($referDomain ne $domainMustBe) || ($httpRefer ne $httpReferMustBe) ) {
  # not a legitimate request from our own business, do nothing.
  printf STDERR "# ERROR: cgi-bin/asr invalid request %d %d %s %s\n", $validIncoming, $extraneousArgs, $referDomain, $httpRefer;
  printf "<p>HTTP_REFERER: %s</p>\n", $referer;
  printf "<p># ERROR: cgi-bin/asr invalid request %d %d %s %s</p>\n", $validIncoming, $extraneousArgs, $referDomain, $httpRefer;
  printf "<h3>err exit at end of asr</h3>\n";
  print "</body></html>\n";
  exit 0;
}


printf "<ul>\n";
printf "<li> name: '%s'</li>\n", $incoming{"name"};
printf "<li>email: '%s'</li>\n", $incoming{"email"};
printf "<li>asmId: '%s'</li>\n", $incoming{"asmId"};
printf "<li>betterName: '%s'</li>\n", $incoming{"betterName"};
printf "<li>comment: '%s'</li>\n", $incoming{"comment"};
printf "</ul>\n";

my $DS=`date "+%F %T"`;
chomp $DS;

my $cleanEmail = $incoming{"email"};
$cleanEmail =~ s/@/ at /;
$cleanEmail =~ s/\./ dot /g;

open (FH, "|/usr/sbin/sendmail -f \"${bounceAddr}\" -t -oi");
printf FH "To: %s
Reply-to: %s
Return-path: %s
Cc: %s
Subject: assembly request: %s

name: '%s'
email: '%s'
asmId: '%s'
betterName: '%s'
comment: '%s'

date: '%s'
", $sendTo, $incoming{"email"}, $legitimateFrom, $Cc, $incoming{"asmId"}, decode_entities($incoming{"name"}), decode_entities($cleanEmail), decode_entities($incoming{"asmId"}), decode_entities($incoming{"betterName"}), decode_entities($incoming{"comment"}), ${DS};

close (FH);

# and then send the email to the google group with the cleanEmail in text
if ( $genArkRequestGroup ne $myAddr ) {	# debugging do not send mail twice
open (FH, "|/usr/sbin/sendmail -f \"${bounceAddr}\" -t -oi");
printf FH "To: %s
Reply-to: %s
Return-path: %s
Subject: assembly request: %s

name: '%s'
email: '%s'
asmId: '%s'
betterName: '%s'
comment: '%s'

date: '%s'
", $genArkRequestGroup, $legitimateFrom, $legitimateFrom, $incoming{"asmId"}, decode_entities($incoming{"name"}), decode_entities($cleanEmail), decode_entities($incoming{"asmId"}), decode_entities($incoming{"betterName"}), decode_entities($incoming{"comment"}), ${DS};

close (FH);
}

print "</body></html>\n";
