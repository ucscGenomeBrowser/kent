#!/usr/bin/perl
##
##  gar -- GenArk Request - receive requests for GenArk assemblies
##

use URI::Escape;

# use strict;
# use warnings;

my $httpRefer = "noReference";
my $referDomain = "noDomain";
my $legitimateFrom = 'hclawson@ucsc.edu';
my $httpReferMustBe = "assemblyRequest.html";
my $domainMustBe = "ucsc.edu";
my $sendTo = 'hclawson@ucsc.edu';
my $Cc = 'clayfischer@ucsc.edu';
my $bounceAddr = 'hiram@soe.ucsc.edu';

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

print "</body></html>\n";

__END__

print "<TABLE><TR><TH COLSPAN=2>hgwdev-hiram CGI gar</TH></TR>\n";

foreach $var (sort(keys(%ENV))) {
    $val = $ENV{$var};
    $val =~ s|\n|\\n|g;
    $val =~ s|"|\\"|g;
    print "<TR><TH>${var}</TH><TD>${val}</TD></TR>\n";
}
print "</TABLE>\n";
print "</body></html>\n";
