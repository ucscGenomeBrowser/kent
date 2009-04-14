#!/usr/bin/perl

use strict;
use warnings;
use File::Temp qw/tempfile/;
use Compress::Zlib;
use HTTP::Request::Common;
use LWP;

$HTTP::Request::Common::DYNAMIC_FILE_UPLOAD = 1;

my $request = POST 'http://hgwdev-mikep/cgi-bin/hgData',
    [
 'filename' => 'testpostfile.bed',
 'verbose' => 2,
 'a_file' => ['test.bed']
    ],
    'Content_Type' => 'form-data',
    'Content_Encoding' => 'gzip';


sub transform_upload {
    my $request = shift;
    
    my ($fh, $filename) = tempfile();
    my $cs = gzopen($fh, "wb");
    
    my $request_c = $request->content();
    while (my $data = $request_c->()) { $cs->gzwrite($data); }

    $cs->gzclose();
    close $fh;
    
    open $fh, $filename; binmode $fh;
    $request->content(sub {
 my $buffer;
 if (0 < read $fh, $buffer, 4096) {
     return $buffer;
 } else {

     close $fh;
     return undef;
 }
    });
    $request->content_length(-s $filename);
}

print "starting test\n";
transform_upload($request);

my $browser = LWP::UserAgent->new();
print "issue request\n";
my $response = $browser->request($request);


print "printing response content\n";
print $response->content();
print "success=[".$response->is_success."] status=[".$response->status_line."]\ndone.\n";

