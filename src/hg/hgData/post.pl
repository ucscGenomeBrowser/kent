#!/usr/bin/perl

use strict;
use warnings;
use File::Temp qw/tempfile/;
use Compress::Zlib;
use HTTP::Request::Common;
use LWP;

die "$0 - post a compressed-on-the-fly file\nusage: \n\n$0 POST|PUT hostname filename repNumber [ContentType]" 
    unless scalar(@ARGV)==4 or scalar(@ARGV)==5;
my $METHOD = $ARGV[0];
my $HOST = $ARGV[1];
my $FILE = $ARGV[2];
my $Rep = $ARGV[3];
my $CONTENTTYPE = defined($ARGV[4]) ? $ARGV[4] : "form-data";


$HTTP::Request::Common::DYNAMIC_FILE_UPLOAD = 1;
# curl  -v --data-binary @test.bed http://mikep/g/project/data/wgEncode/Gingeras/Helicos/RnaSeq/Alignments/K562,cytosol,longNonPolyA/hg18?filename=trash/testing.bed

select STDOUT; $| = 1;

# ?project=wgEncode&pi=Gingeras&lab=Helicos&dataType=RnaSeq&view=Alignments&cell=K562&localization=cytosol
# &rnaExtract=longNonPolyA

#my $request = $METHOD eq "POST" ? (POST "http://$HOST/g/track/data/hg18/wgEncodeHelicosRnaSeqAlignmentsK562CytosolLongnonpolya",
my $request = $METHOD eq "POST" ? (POST "http://$HOST/g/project/wgEncode/tracks/hg18/wgEncodeHelicosRnaSeqAlignmentsTestcellNucleus2Longpolya$Rep?type=tagAlign&pi=Gingeras&lab=Helicos&datatype=RnaSeq&variables=testCell,nucleus2,longPolyA&view=Alignments&rep=$Rep&sourceFile=$FILE",
    [
#  'verbose' => 2,
 'a_file' => [ $FILE ]
    ],
    'Content_Type' => $CONTENTTYPE,
    'Content_Encoding' => 'gzip')
 : 
  (PUT "http://$HOST/g/project/wgEncode/tracks/wgEncodeGingerasHelicosRnaSeqAlignmentsK562CytosolLongNonPolyA$Rep/hg18?filename=testing.bed",
    [
#  'verbose' => 2,
 'a_file' => [ $FILE ]
    ],
    'Content_Type' => $CONTENTTYPE,
    'Content_Encoding' => 'gzip');


sub transform_upload {
    my $request = shift;
    
    my ($fh, $filename) = tempfile();
    my $cs = gzopen($fh, "wb");
    print "\n===file $filename open===\n".localtime()."\n";

    my $request_c = $request->content();
    while (my $data = $request_c->()) { $cs->gzwrite($data); }
    print "\n===gzip $filename closing===\n".localtime()."\n";

    $cs->gzclose();
    print "\n===file $filename closing===\n".localtime()."\n====================================\n";
    close $fh;
    
    open $fh, $filename; binmode $fh;
    $request->content(sub {
 my $buffer;
 if (0 < read $fh, $buffer, 4096) {
    print ".";
     return $buffer;
 } else {

     close $fh;
     return undef;
 }
    });
    print "\n===request content length===\n".localtime()."\n====================================\n";
    $request->content_length(-s $filename);
}

$request->authorization_basic('mikep', 'gringo');

print "starting test\n====================================\n".localtime()."\n====================================\n";
if ($FILE =~ /.gz/) {
    #$HTTP::Request::Common::DYNAMIC_FILE_UPLOAD = 1;
} else {
transform_upload($request);
}

my $browser = LWP::UserAgent->new();
print "issue request\n====================================\n".localtime()."\n====================================\n";
my $response = $browser->request($request);


print "printing response content\n====================================\n".localtime()."\n====================================\n";
print $response->content();
print "\n====================================\n".localtime()."\n====================================\n";
print "Location: [".$response->header("Location")."]\n"if $response->header("Location");
print "success=[".$response->is_success."] status=[".$response->status_line."]\ndone.\n";

