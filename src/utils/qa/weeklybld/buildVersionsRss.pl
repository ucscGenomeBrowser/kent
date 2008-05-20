#!/usr/bin/env perl

# Parse builds/versions-list.html etc. and automatically create versions.xml RSS feed

# Format based on RSS 2.0; see http://cyber.law.harvard.edu/rss/rss.html

use strict;

use Date::Manip;
use HTTP::Date;

my $debug = 0;
my $urlPrefix = "http://genome-test.cse.ucsc.edu/builds";

sub usage
{
    print STDERR <<END;
buildVersionsRss - Output an RSS feed based on data parsed out from versions-list.html.
usage:
    buildVersionsRss.pl <versions file name>

example:
    buildVersionsRss.pl ~/browser/builds/versions-list.html > /usr/local/apache/htdocs/builds/versions.xml 
END
    exit(1);
}

my $versionsFile = shift(@ARGV) or usage();

if($versionsFile =~ /(.*)\/([^\/]+)$/) {
    $versionsFile = $2;
    chdir($1) or die "Cannot chdir into $1: $!";
}

open(VERSIONS, $versionsFile) or die "cannot open file: $versionsFile: $!";

print <<END;
<rss version="2.0">
<channel>
<title>UCSC Genome Browser Versions</title>
<link>$urlPrefix/versions.html</link>
<description />
<language>en-us</language>
<copyright></copyright>
<lastBuildDate>@{[time2str(time())]}</lastBuildDate>
END
while(<VERSIONS>)
{
    if(/versions-all\/v(\d+)-(final|preview)\.html/) {
        my ($version, $prefix) = ($1, $2);
        print STDERR "$1\t$2\n" if($debug);
        my $file = "versions-all/v$version-$prefix.html";
        if(open(HTML, $file)) {
            my $html = join("\n", <HTML>);
            my ($title, $date);

            # Parse out <H1>name - version</H1>
            if($html =~ /<H1>\s*(\S+)\s*<\/H1>/) {
                # title only
                $title = $1;
                # Try to extract date from CVS log, but ignore those that were entered via bulk entry on 2006-07-26
                my $cvsLog = `cvs log $file`;
                if($cvsLog =~ /date: (.*?);/ && !($cvsLog =~ /Adding these to CVS for safekeeping/)) {
                    printf STDERR "found title: $title in cvsLog\n" if($debug);
                    $date = str2time($1);
                    my @tmp = gmtime($date);
                    my @months = qw(Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec);
                    $title = "$title - " . $tmp[3] . " " . $months[$tmp[4]] . ". " . (1900 + $tmp[5]);
                }
            } else {
                my $originalDate;
#                if ($html =~ /<H1>\s*(\S+)\s+-\s+(.+)<\/H1>/) {
                if ($html =~ /<H1>\s*(\S+)\s+-(.+?)\s*<\/H1>/) {
                    # Single line; e.g. <H1> v140-final - 08-15-2006 </H1> in v140-final.html
                    $title = $1;
                    $originalDate = $2;
                    $title = "$title - $originalDate";
                } elsif($html =~ /<H1>\s*(\S+)\s+-\s+(.*?)\s*\n/) {
                    # newer multi-line format (e.g. v180-final.html)
                    $title = $1;
                    $originalDate = $2;
                    $title = "$title - $originalDate";
                }
                if($originalDate) {
                    # try to deal with all the date formats donna uses.
                    my $str;
                    if($date = ParseDate($originalDate)) {
                        # e.g. "12 June 2007"
                        $str = $date;
                        $str =~ s/(\d{8})(.*)/$1T$2/;
                    } else {
                        if($originalDate =~ /(\d+)(\D+)\.\s+(\d{4})/) {
                            # e.g. "16 Jan. 2007"
                            $str = "$2 $1 $3";
                        } elsif($originalDate =~ /(\d+)-(\d+)-(\d{4})/) {
                            # Handle "08-15-2006"
                            $str = "$3-$1-$2";
                        } else {
                            $str = $originalDate; 
                        }
                    }
                    if(!($date = str2time($str))) {
                        die "Couldn't parse $str into HTTP::Date style date";
                    }
                }
            }
            if($title && $date) {
                print STDERR "$title\n" if($debug);
                
                # RSS 2.0 requires RFC 822 format (which is similar to RFC 1123, which
                # supported by HTTP::Date).
                $date = time2str($date);
                print <<END;
<item>
    <title>$title</title>
    <link>$urlPrefix/$file</link>
    <pubDate>$date</pubDate>
</item>
END
            } elsif (($version <= 137) || ($version == 138 && $prefix eq 'preview')) {
                # file syntax is not consistent before 140-final and cvs logs are either missing or due to a mass checkin
                print STDERR "Ignoring older version: $version-$prefix\n" if($debug);
            } else {
                if($debug) {
                    print STDERR "Couldn't extract date from $file\n";
                } else {
                    die "Couldn't extract date from $file";
                }
            }
        } else {
            if($debug) {
                print STDERR "can't open file: $file\n";
            } else {
                die "can't open file: $file: $!";
            }
        }
    }
}
print <<END;
</channel>
</rss>
END
