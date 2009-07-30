#!/usr/local/bin/perl

# check random sequences from tagAlign files to make sure the corresponding fastq contains those sequences.

use strict;

use Getopt::Long;

use lib "/cluster/bin/scripts";
use Encode;
use SafePipe;

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
           not the default behavior of our data providers).

-num       number of tags to check (default is 10)

-touch     Touch fastq file so it has the same mtime as tagAlign file.
END
    exit(0);
}

my $useReverseLogic;
my $num=10;
my $help;
my $verbose;
my $debug;
my $touch;
my $maxLines;       # this is a debugging hack
GetOptions("reverse" => \$useReverseLogic, "help" => \$help, "num=i" => \$num, "verbose" => \$verbose,
           "touch" => \$touch, "debug" => \$debug, "maxLines=i" => \$maxLines);

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
                $lineNumber++;
                my ($seq, $strand);
                if(/^\S+\s+\S+\s+\S+\s+(\S+)\s+\S+\s+(\S+)/) {
                    $seq = $1;
                    $strand = $2;
                } else {
                    die "Linenumber $lineNumber: invalid tagAlign file: $file";
                }
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
                last if(defined($maxLines) && $lineNumber >= $maxLines);
            }
            $fh->close();
            if($touch) {
                `/bin/touch -r $file $fastq`;
            }
            # XXXX add smart auto-sensing reverse logic
            # e.g. AGTGCTACTGCGAGCTAAAAAGGATCCAGATAA in wgEncodeChromatinMap/wgEncodeUtaustinChIPseqRawDataGm12878Input.fastq.gz

            # For efficiency, we zgrep all $num tags at once; %uniq stuff is to handle
            # edge case where we have less than $num unique tags because the tagAlign is very short.
            my %uniq;
            for my $rec (@list) {
                $uniq{$rec->{SEQ}}++;
            }
            my @uniq = keys %uniq;
            my $str = join("|", @uniq);
            my @cmds;
            push(@cmds, "/usr/bin/zgrep -e '\($str\)' $fastq");
            my $safe = SafePipe->new(CMDS => \@cmds, DEBUG => $debug);
            my @missed;
            if(my $err = $safe->exec()) {
                @missed = keys %uniq;
            } else {
                for (split(/\n/, $safe->{STDOUT})) {
                    # should be just the sequence, but I'm trying to be safe here.
                    if(/([A-Za-z]+)/) {
                        delete $uniq{$1};
                    }
                }
                @missed = keys %uniq;
            }
            if(@missed) {
                print STDERR "Couldn't find the following sequences from $file in $fastq\n" . join("\n", @missed) . "\n";
            } else {
                print STDERR "$file is ok\n";
            }
        } else {
            print STDERR "Couldn't find fastq file for $file\n";
        }
    } else {
        die "file: $file does not exit\n";
    }
}
