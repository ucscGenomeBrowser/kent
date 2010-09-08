#!/usr/local/bin/perl
## Before using this script in the cgi-bin/ directory, verify it functions
##	OK from the command line:
##	./printEnv.pl
## should print out your shell environment variables.
## You may need to change the first line to the location of your perl.
## To find out where your perl is, in bash use the 'type perl' command,
##	in csh/tcsh use the 'which perl' command
##
##  printenv -- demo CGI program which just prints its environment
##
##	$Id: printEnv.pl,v 1.1 2010/04/01 18:55:18 hiram Exp $
##
## You may need to change the path to perl above if it is located elsewhere
##	place this script in your cgi-bin/ directory and open a URL to
##	it from your WEB browser:
##	http://your.server.edu/cgi-bin/printEnv.pl
##
## To debug hgTracks problems, uncomment the three system calls and
##	the exit 0 statements.  Then use a URL to this script:
##	http://your.server.edu/cgi-bin/printEnv.pl
## If hgTracks is OK, you should see the default browser display.
##	If hgTracks is failing, check the resulting /tmp/hgTracks.txt
##	file to see if any error messages are output by hgTracks for
##	clues to the problem.

## system("date >> /tmp/hgTracks.txt");
## system("chmod 666 /tmp/hgTracks.txt");
## system("./hgTracks 2>> /tmp/hgTracks.txt");
## exit 0;

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
