#!/usr/local/bin/perl

# check 10 random sequences from tagAlign files to make sure the
# corresponding fastq contains those sequences.

use strict;

use Getopt::Long;

use lib "/cluster/bin/scripts";
use Encode;

sub usage
{
    print STDERR <<END;
Do following sanity check of all listed tagAlign files:

Choose 10 tags at random from the tagAlign file and confirm that they are in the
corresponding fastq file.

We assume that fastq file name has "Alignments" replaced with "RawData"; e.g.:

           wgEncodeYaleChIPseqRawDataGm12878Input.fastq.gz is fastq for wgEncodeYaleChIPseqAlignmentsGm12878Input.tagAlign.gz

Options:

-reverse   If true, tags on reverse strand are reversed before we search for them in fastq (this is apparently
           not the default behavior).

-num       number of tags to check (default is 10)

-touch     Touch fastq file so it has the same mtime as tagAlign file.


END
    exit(0);
}

my $useReverseLogic;
my $num=10;
my $help;
my $verbose;
my $touch;
GetOptions("reverse" => \$useReverseLogic, "help" => \$help, "num=i" => \$num, "verbose" => \$verbose, "touch" => \$touch);

if($help) {
    usage();
}

for my $file (@ARGV) {
    if(-e $file) {
        my $fastq = $file;
        $fastq =~ s/Alignments/RawData/;
        $fastq =~ s/tagAlign/fastq/;
        if(-e $fastq) {
            print STDERR "Checking $file\n" if($verbose);
            my @list;
            my $fh = Encode::openUtil($file);
            my $lineNumber = 0;
            while(<$fh>) {
                my @line = split(/\s+/);
                $lineNumber++;
                my $seq = $line[3];
                my $strand = $line[5];
                $seq =~ s/\./N/g;
                if($useReverseLogic && $strand eq '-') {
                    $seq =~ tr/ATGCatgc/TACGtacg/;
                    $seq = reverse($seq);
                }
                # User Kernighan's algo for picking random lines from a file
                for (my $i=0;$i<$num;$i++) {
                    if(int(rand($lineNumber)) == 0) {
                        $list[$i] = {SEQ => $seq, STRAND => $strand};
                    }
                }
            }
            $fh->close();
            my $errors = 0;
            if($touch) {
                `/bin/touch -r $file $fastq`;
            }
            for my $rec (@list) {
                my $seq = $rec->{SEQ};
                # XXXX add smart auto-sensing reverse logic
                if(system("/usr/bin/zgrep -q $seq $fastq")) {
                    print STDERR "Couldn't find $seq from $file in $fastq\n";
                    $errors++;
                    last;
                }
            }
            if(!$errors) {
                print STDERR "$file is ok\n";
            }
        } else {
            print STDERR "Couldn't find fastq file for $file\n";
        }
    } else {
        die "file: $file does not exit\n";
    }
}
