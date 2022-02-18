#!/usr/bin/perl
##
##  gar -- GenArk Request - receive requests for GenArk assemblies
##

use URI::Escape;

# use strict;
# use warnings;

print "Content-type: text/html\n\n";

print "<HTML><HEAD><TITLE>GenArk Request assembly build</TITLE></HEAD>\n";
print "<BODY>\n";

# QUERY_STRING    name=some%20name&email=some@email.com&asmId=GCF_000951035.1_Cang.pa_1.0

my %incoming = (
  "name" => "noName",
  "email" => "noEmail",
  "asmId" => "noAsmId",
  "betterName" => "noBetterName",
  "comment" => "noComment",
);

if (defined($ENV{"QUERY_STRING"})) {
  my $qString = $ENV{"QUERY_STRING"};
  my @idVal = split("&", $qString);
  foreach $id (@idVal) {
    my ($tag, $value) = split("=", $id, 2);
    $incoming{$tag} = uri_unescape( $value ) if (defined($value));
  }
}

printf "<ul>\n";
printf "<li> name: '%s'</li>\n", $incoming{"name"};
printf "<li>email: '%s'</li>\n", $incoming{"email"};
printf "<li>asmId: '%s'</li>\n", $incoming{"asmId"};
printf "<li>betterName '%s'</li>\n", $incoming{"betterName"};
printf "<li>comment '%s'</li>\n", $incoming{"comment"};
printf "</ul>\n";

my $DS=`date "+%F %T"`;
chomp $DS;

open (FH, "|/usr/sbin/sendmail -t -oi");
printf FH "To: hclawson\@ucsc.edu
From: %s
Subject: gar request: %s

name: '%s'
email: '%s'
asmId: '%s'
betterName: '%s'
comment: '%s'

date: '$DS'
", $incoming{"email"}, $incoming{"asmId"}, $incoming{"name"}, $incoming{"email"}, $incoming{"asmId"}, $incoming{"betterName"}, $incoming{"comment"};

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
