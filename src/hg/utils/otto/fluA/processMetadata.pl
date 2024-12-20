#!/usr/bin/env perl

# Parse influenza A metadata downloaded from GenBank vi an NCBI Virus query.  Most rows have
# values in the date, serotype, strain and segment columns, but when they don't, try to extract them
# from the verbose GenBank title.

use warnings;
use strict;

sub strainFromName($$) {
  my ($isolate, $title) = @_;
  if ($isolate =~ m@^A/\w+.*/\d+ ?\(?H\d+(N\d+[^)^ ]*)?@) {
    return $isolate;
  } elsif ($title =~ m@\b(A/\w+.*/\d+ ?_?\(?H\d+(N\d+[^)^ ]*\)?)?)@ ||
           $title =~ m@\b(A/\w+.*\(H\d+N\d+\))@) {
    return $1;
  } elsif ($isolate =~ m@^A/\w+.*/\d+@) {
    return $isolate;
  } elsif ($title =~ m@\b(A/\w+.*/\d+)@) {
    return $1;
  } elsif ($isolate =~ m@/@) {
    return $isolate;
  } elsif ($isolate) {
    return $isolate;
  }
  if ($isolate || ! $title =~ /^Influenza A virus genome assembly/) {
    print STDERR "No dice for isolate='$isolate', title='$title'\n";
  }
  return "";
} # strainFromName


sub serotypeFromName($) {
  my ($strain) = @_;
  if ($strain =~ m@\((H\d+N\d+)\).*\((H\d+N\d+)\)@) {
    if ($1 eq $2) {
      return $1;
    } else {
      # Recombinant of different serotypes, submitter didn't specify serotype --> unknown
      return "";
    }
  } elsif ($strain =~ m@\((H\d+(N\d+[^)^ ]*)?)\)@) {
    return $1;
  } elsif ($strain =~ m@\b(H\d+(N\d+[^)^ ]*)?)@) {
    return $1;
  }
  return "";
} # serotypeFromName


sub segmentFromTitle($) {
  my ($title) = @_;
  if ($title =~ m/ha?ea?m?m[ae]g?glut/i || $title =~ m/surface glycoprotein/i) {
    return 4;
  } elsif ($title =~ m/neurami/i) {
    return 6;
  } elsif ($title =~ m/polymerase basic (protein|subunit) 2/i ||
           $title =~ m/polymerase (protein )?2/i) {
    return 1;
  } elsif ($title =~ m/polymerase basic (protein|subunit) 1/i ||
           $title =~ m/polymerase (protein )?1/i) {
    return 2;
  } elsif ($title =~ m/(polymerase acid|polymerase (protein ?)A)/i) {
    return 3;
  } elsif ($title =~ m/(nucleoprotein|nucleocapsid)/i) {
    return 5;
  } elsif ($title =~ m/matrix/i) {
    return 7;
  } elsif ($title =~ m/non-?structural protein/i) {
    return 8;
  } elsif ($title =~ m/\bPB2|P2\b/i) {
    return 1;
  } elsif ($title =~ m/\bPB1\b/i) {
    return 2;
  } elsif ($title =~ m/\bPA\b/i) {
    return 3;
  } elsif ($title =~ m/\bHA[12]?\b/i) {
    return 4;
  } elsif ($title =~ m/\bNP\b/i) {
    return 5;
  } elsif ($title =~ m/\bNA\b/i) {
    return 6;
  } elsif ($title =~ m/\bM[12A]?\b/i) {
    return 7;
  } elsif ($title =~ m/\bNS[12]?\b/i) {
    return 8;
  }
  return "";
} # segmentFromName


sub yearFromName($) {
  my ($strain) = @_;
  if ($strain =~ m@/(\d{4})\b[^/]*$@) {
    return $1;
  } elsif ($strain =~ m@/(\d{2})\b[^/]*$@) {
    my $y2 = $1;
    if ($y2 > 24) {
      return 1900 + $y2;
    }
  }
  return "";
} # yearFromName

my %segName = ( '1' => 'PB2',
                '2' => 'PB1',
                '3' => 'PA',
                '4' => 'HA',
                '5' => 'NP',
                '6' => 'NA',
                '7' => 'MP',
                '8' => 'NS',
                'M' => 'MP',
                'MA'=> 'MP'
              );


while (<>) {
  chomp;
  my @w = split(/\t/);
  # If trailing columns are empty, that makes too few columns; make sure we have the right number.
  my $colCount = @w;
  if ($colCount < 17) {
    for (my $i = $colCount;  $i < 17;  $i++) {
      $w[$i] = "";
    }
  }
  my ($dateIx, $strainIx, $serotypeIx, $segmentIx) = (4, 14, 15, 16);
  my ($acc, $isolate, $title) = ($w[0], $w[1], $w[11]);
  if (! $w[$strainIx]) {
    $w[$strainIx] = strainFromName($isolate, $title);
  }
  if (! $w[$serotypeIx] || $w[$serotypeIx] eq "A") {
    $w[$serotypeIx] = serotypeFromName($w[$strainIx]);
  }
  if (! $w[$segmentIx]) {
    $w[$segmentIx] = segmentFromTitle($title);
  }
  $w[$segmentIx] =~ s/RNA //;
  $w[$segmentIx] =~ s/segment //;
  if (exists $segName{$w[$segmentIx]}) {
    $w[$segmentIx] = $segName{$w[$segmentIx]};
  }
  if (! $w[$dateIx]) {
    $w[$dateIx] = yearFromName($w[$strainIx]);
  }
  print join("\t", @w) . "\n";
}
