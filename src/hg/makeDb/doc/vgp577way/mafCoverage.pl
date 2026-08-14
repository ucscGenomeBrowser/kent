#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 2) {
  printf STDERR "usage: mafCoverage.pl referenceDb file.maf\n";
  printf STDERR "measures the amount of the reference genome sequence\n";
  printf STDERR "that is 'covered', or matched to query genome sequence\n";
  printf STDERR "For each block in the input maf file, count the amount\n";
  printf STDERR "of sequence for the reference sequence in this block\n";
  printf STDERR "that has any type of corresponding sequence in each\n";
  printf STDERR "query sequence.  A 'coverage' measurement is thereby\n";
  printf STDERR "calculated for each query sequence.\n";
  exit 255;
}

sub vmPeak() {
  my $pid = $$;
  my $vmPeak = `grep -m 1 -i vmpeak /proc/$pid/status`;
  chomp $vmPeak;
  printf STDERR "# %s\n", $vmPeak;
}

my $refDb = shift;
my $mafFile = shift;
my $blockCount = 0;
my $totalBlockSize = 0;
my $totalInsertSize = 0;

if ($mafFile =~ m/.gz$/) {
  open (MF, "zcat $mafFile|") or die "can not zcat $mafFile";
} else {
  open (MF, "<$mafFile") or die "can not read $mafFile";
}

my %refSummary; # key is chrom name for the reference sequence
              # value is a hash pointer with keys:
              # 'refSize' value is size of this reference chromosome
              # 'query assembly name'  value is a hash pointer with key:
              # 'query sequence name' value is a hash pointer with keys:
              # 'querySize' value is size of this query chromosome
              # 'matched' value is amount of reference matched by this query
              # 'misMatched' value is amount of mis-matches, query or target
my @refNTs;
my $srcChr = "";

while (my $line = <MF>) {
  chomp $line;
  if ($line =~ m/^s ${refDb}./) {
     my (undef, $refSrc, $refStart, $refSize, $refStrand, $refChrSize, $refText) = split('\s+', $line);
     if ($refSrc =~ m/^GC/) {
       my @a = split(/\./, $refSrc);
       $srcChr = join(".", @a[2..$#a]);
     } else {
       (undef, $srcChr) = split(/\./, $refSrc, 2);
     }
#     printf STDERR "# %s -> %s\n", $refSrc, $srcChr;
     if (!defined($refSummary{$srcChr})) {
       my %h;
       $h{'refSize'} = $refChrSize;
       $refSummary{$srcChr} = \%h;
     }
     my $refPtr = $refSummary{$srcChr};
     if ($refPtr->{'refSize'} != $refChrSize) {
        printf "ERROR: inconsistent reference chr size, was $refPtr->{'refSize'} vs. now $refChrSize\n";
        exit 255;
     }
     ++$blockCount;
#     printf STDERR "# %s.%s\t%d\n", $refDb, $srcChr, $refPtr->{'refSize'};
     my $totalSize = length($refText);
     my $noInserts = $refText;
     $noInserts =~ s/-//g;
     my $noInsertSize = length($noInserts);
     @refNTs = ();
     @refNTs = split(//, uc($refText));
     my $NTsCount = scalar(@refNTs);
     my $insertCount = $totalSize - $noInsertSize;
     $refPtr->{'inserts'} += $insertCount;
     $refPtr->{'considered'} += $noInsertSize;
     $totalBlockSize += $totalSize;
     $totalInsertSize += $insertCount;
     my @sampleRef = @refNTs[0..6];
#     printf "%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n", $refSrc, $refStart, $refSize, $refChrSize, $totalSize, $noInsertSize, $insertCount, $NTsCount, join('', @sampleRef);
  } elsif ($line =~ m/^s /) {
     my (undef, $qSrc, $qStart, $qSize, $qStrand, $qChrSize, $qText) = split('\s+', $line);
     my $qAsmName = "n/a";
     my $qChrName = "n/a";
     if ($qSrc =~ m/^GC/) {
       my @a = split(/\./, $qSrc);
       $qChrName = join(".", @a[2..$#a]);
       $qAsmName = join(".", @a[0..1]);
     } else {
       ($qAsmName, $qChrName) = split(/\./, $qSrc, 2);
     }
#     printf STDERR "# %s -> %s %s\n", $qSrc, $qAsmName, $qChrName;
     my $refPtr = $refSummary{$srcChr};
     if (!defined($refPtr->{$qAsmName})) {
        my %h;
        $refPtr->{$qAsmName} = \%h;
     }
     my $qPtr = $refPtr->{$qAsmName};
     if (!defined($qPtr->{$qChrName})) {
        my %h;
        $h{'querySize'} = $qChrSize;
        $qPtr->{$qChrName} = \%h;
     }
     my $qChrPtr = $qPtr->{$qChrName};
     if ($qChrPtr->{'querySize'} != $qChrSize) {
        printf "ERROR: inconsistent query chr size, was $qChrPtr->{'querySize'} vs. now $qChrSize\n";
        exit 255;
     }
#     printf STDERR "# %s.%s\t%d\n", $qAsmName, $qChrName, $qChrPtr->{'querySize'};
     my $totalSize = length($qText);
     my $noInserts = $qText;
     $noInserts =~ s/-//g;
     my $noInsertSize = length($noInserts);
     my $insertCount = $totalSize - $noInsertSize;
     my @qNTs = ();
     @qNTs = split(//, uc($qText));
     die "unequal text sizes for $qSrc vs $srcChr" if (scalar(@qNTs) != scalar(@refNTs));
     my @sampleQ = @qNTs[0..6];
     my $matchedSequence = 0;
     my $misMatch = 0;
     my @matchRef = ();	# reference sequence matching
     my @matchQ = ();	# query sequence matching
     for (my $i = 0; $i < scalar(@qNTs); ++$i) {
       next if ( ($refNTs[$i] eq "-") || ($qNTs[$i] eq "-") );
       push (@matchRef, $refNTs[$i]);
       if ($refNTs[$i] eq $qNTs[$i]) {
         push (@matchQ, ".");
         ++$matchedSequence;
       } else {
         ++$misMatch;
         push (@matchQ, $qNTs[$i]);
       }
     }
     $qChrPtr->{'matched'} += $matchedSequence;
     $qChrPtr->{'misMatched'} += $misMatch;

#     printf "%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\t%d\n", $qSrc, $qStart, $qSize, $qChrSize, $totalSize, $noInsertSize, $insertCount, scalar(@qNTs), join('', @sampleQ), $matchedSequence;
#     printf "%s\n", join('', @matchRef);
#     printf "%s\n", join('', @matchQ);
  }
}

close (MF);

# my %refSummary; # key is chrom name for the reference sequence
              # value is a hash pointer with keys:
              # 'refSize' value is size of this reference chromosome
              # 'query assembly name'  value is a hash pointer with key:
              # 'query sequence name' value is a hash pointer with keys:
              # 'querySize' value is size of this query chromosome
              # 'matched' value is amount of reference matched by this query
              # 'misMatched' value is amount of mis-matches, query or target

foreach my $refChr (sort keys %refSummary) {
  my $refPtr = $refSummary{$refChr};
  foreach my $qAsmName (sort keys %$refPtr ) {
    next if ($qAsmName eq "refSize");
    next if ($qAsmName eq "inserts");
    next if ($qAsmName eq "considered");
    my $qPtr = $refPtr->{$qAsmName};
    foreach my $qChrName (sort keys %$qPtr) {
       my $qChrPtr = $qPtr->{$qChrName};
       my $refCoverPerCent = 100.0 * $refPtr->{'considered'} / $refPtr->{'refSize'};
       my $qCoveredRef = 100.0 * $qChrPtr->{'matched'} / $refPtr->{'considered'};
       my $qMisMatch = 100.0 * $qChrPtr->{'misMatched'} / $refPtr->{'considered'};
       printf "%s.%s\t%d %d %d %%%.2f\t%s.%s %d %d %d %%%.2f %%%.2f\n", $refDb, $refChr, $refPtr->{'refSize'}, $refPtr->{'inserts'}, $refPtr->{'considered'}, $refCoverPerCent, $qAsmName, $qChrName, $qChrPtr->{'querySize'}, $qChrPtr->{'matched'}, $qChrPtr->{'misMatched'}, $qCoveredRef, $qMisMatch;
    }
  }
}

printf "# total blocks: %d, noInsertSize %d = total size: %d - inserts %d\n# average block size: %d = %d / %d\n",
  $blockCount, $totalBlockSize - $totalInsertSize, $totalBlockSize, $totalInsertSize, $totalBlockSize / $blockCount, $totalBlockSize, $blockCount;

printf "#refDb.chr size inserts considered refPerCent qAsmName.qChr qSize matched misMatched qCoveredRef%% qMisMatch%%\n";

vmPeak();

__END__
#        printf "%s.%s\t%d\t%s.%s\t%d\n", $refDb, $refChr, $refPtr->{'refSize'}, $qAsmName, $qChrName,  $qChrPtr->{'querySize'};
