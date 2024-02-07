#!/usr/bin/perl
##
##  gar -- GenArk Request - receive requests for GenArk assemblies
##

use URI::Escape;

# use strict;
# use warnings;

my $httpRefer = "noReference";
my $referDomain = "noDomain";
my $domainMustBe = "ucsc.edu";
my $httpReferMustBe = "assemblyRequest.html";
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

if (defined($ENV{'HTTP_REFERER'})) {
  my @a = split('/', $ENV{'HTTP_REFERER'});
  $httpRefer = $a[-1];  # should be "assemblyRequest.html"
  $httpRefer =~ s/\?.*//;	# remove arguments if present
  my @b = split('\.', $a[-2]);
  $referDomain = "$b[-2].$b[-1]";	# should be "ucsc.edu"
}

print "Content-type: text/html\n\n";

print "<html><head><title>GenArk Request assembly build</title></head>\n";
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

if (defined($ENV{"QUERY_STRING"})) {
  my $qString = $ENV{"QUERY_STRING"};
  my @idVal = split("&", $qString);
  foreach $id (@idVal) {
    my ($tag, $value) = split("=", $id, 2);
    # only accept known inputs, the five defined above for %incoming defaults
    if (defined($incoming{$tag}) && defined($value)) {
      $incoming{$tag} = uri_unescape( $value );
      ++$validIncoming;
    }
    ++$extraneousArgs if (!defined($incoming{$tag}));
  }
}

if ( ($validIncoming != 5) || ($extraneousArgs > 0) || ($referDomain ne $domainMustBe) || ($httpRefer ne $httpReferMustBe) ) {
  # not a legitimate request from our own business, do nothing.
  printf STDERR "# ERROR: cgi-bin/gar invalid something: %d %d %s %s\n", $validIncoming, $extraneousArgs, $referDomain, $httpRefer;
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


open (FH, "|/usr/sbin/sendmail -f \"${bounceAddr}\" -t -oi");
printf FH "To: %s
Reply-to: %s
Return-path: %s
Cc: %s
Subject: gar request: %s

name: '%s'
email: '%s'
asmId: '%s'
betterName: '%s'
comment: '%s'

date: '%s'
", $sendTo, $incoming{"email"}, $legitimateFrom, $Cc, $incoming{"asmId"}, $incoming{"name"}, $incoming{"email"}, $incoming{"asmId"}, $incoming{"betterName"}, $incoming{"comment"}, ${DS};

close (FH);

my $cleanEmail = $incoming{"email"};
$cleanEmail =~ s/@/ at /;
$cleanEmail =~ s/\./ dot /g;

# and then send the email to the google group with the cleanEmail in text
open (FH, "|/usr/sbin/sendmail -f \"${bounceAddr}\" -t -oi");
printf FH "To: %s
Reply-to: %s
Return-path: %s
Subject: gar request: %s

name: '%s'
email: '%s'
asmId: '%s'
betterName: '%s'
comment: '%s'

date: '%s'
", $genArkRequestGroup, $legitimateFrom, $legitimateFrom, $incoming{"asmId"}, $incoming{"name"}, $cleanEmail, $incoming{"asmId"}, $incoming{"betterName"}, $incoming{"comment"}, ${DS};

close (FH);

print "</body></html>\n";
