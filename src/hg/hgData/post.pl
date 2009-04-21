#!/usr/bin/perl

use strict;
use warnings;
use File::Temp qw/tempfile/;
use Compress::Zlib;
use HTTP::Request::Common;
use LWP;

die "$0 - post a compressed-on-the-fly file\nusage: \n\n$0 hostname filename " unless scalar(@ARGV)==2;
my $HOST = $ARGV[0];
my $FILE = $ARGV[1];

$HTTP::Request::Common::DYNAMIC_FILE_UPLOAD = 1;
# curl  -v --data-binary @test.bed http://mikep/g/project/data/wgEncode/Gingeras/Helicos/RnaSeq/Alignments/K562,cytosol,longNonPolyA/hg18?filename=trash/testing.bed

my $request = POST "http://$HOST/g/project/data/wgEncode/Gingeras/Helicos/RnaSeq/Alignments/K562,cytosol,longNonPolyA/hg18",
    [
#  'verbose' => 2,
 'a_file' => [ $FILE ]
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

