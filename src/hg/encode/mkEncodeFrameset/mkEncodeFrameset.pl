#!/usr/bin/perl -w

# mkEncodeFrameset - generate ENCODE region frameset with browser,
#                            optionally, with specified position
# 10/03 kate
# usage: mkEncodeFrameset?position=N

#use strict;

use CGI qw(:standard);

# HTTP header
print header();

#print start_html('ENCODE');
# NOTE: this breaks in Netscape 4.75 (croaks on body tag)
print "<html><head><title>ENCODE</title></head>\n";

my $position;
if (param('position')) {
    $position = param('position');
} else {
    $position = "chr1:1-10000";
}

#<frameset cols="27%,73%">
# print invariant part of frameset
print <<EOF;
<frameset cols="400,1200">
<frame name="dir" src="/ENCODE/dir.html"> 
<frame name="browser" src="hgTracks?db=hg16&position=$position"> 
</frameset>
EOF

print end_html();



