#!/usr/local/bin/perl
##
##  printenv -- demo CGI program which just prints its environment
##
##	$Id: printEnv.pl,v 1.1 2010/04/01 18:55:18 hiram Exp $
##
##	You may need to change the path to perl above if it is located elsewhere
##	place this script in your cgi-bin/ directory and open a URL to
##	it from your WEB browser:
##	http://your.server.edu/cgi-bin/printEnv.pl
##

print "Content-type: text/html\n\n";

print "<HTML><HEAD><TITLE>testing CGI</TITLE></HEAD>\n";
print "<BODY>\n";
print "<TABLE><TR><TH COLSPAN=2>testing CGI</TH></TR>\n";

foreach $var (sort(keys(%ENV))) {
    $val = $ENV{$var};
    $val =~ s|\n|\\n|g;
    $val =~ s|"|\\"|g;
    print "<TR><TH>${var}</TH><TD>${val}</TD></TR>\n";
}
print "</TABLE></BODY></HTML>\n";
