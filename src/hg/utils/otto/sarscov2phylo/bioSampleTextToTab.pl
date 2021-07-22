#!/usr/bin/env perl

use warnings;
use strict;

sub isReal($) {
  my ($val) = @_;
  return ($val &&
          (lc($val) ne "missing") &&
          (lc($val) ne "unknown") &&
          (lc($val) ne "not applicable") &&
          (lc($val) ne "not collected") &&
          (lc($val) ne "not provided") &&
          (lc($val) ne "restricted access"));
}

my %attribs = ();

while (<>) {
  chomp;
  if (/^Identifiers: BioSample: (\w+)(; Sample name: ([^;]+))?(; SRA: (\w+))?/) {
    my ($acc, $sampleName, $sra) = ($1, $3, $5);
    $attribs{__acc} = $acc;
    $attribs{__sampleName} = $sampleName if ($sampleName);
    $attribs{__sra} = $sra if ($sra);
  } elsif (/^Identifiers: /) {
    die "Can't parse Identifiers line $.:\n$_\t";
  } elsif (/^    \/([^=]+)="(.+)"$/) {
    my ($attr, $val) = ($1, $2);
    if (isReal($val)) {
      $attribs{$attr} = $val;
    }
  } elsif (/^    \/([^=]+)=""$/) {
    # empty value; ignore.
    next;
  } elsif (/^    \//) {
    die "Can't parse attribute line $.:\n$_\t";
  } elsif (/^(EPI_ISL_\d+)/) {
    $attribs{__epi} = $1;
  } elsif (/^Accession: (\w+)\sID: (\d+)/) {
    # Last line of record; reconcile whatever attributes were accumulated with the columns that
    # we want to define.
    my ($acc, $gi) = ($1, $2);
    die "acc mismatch '$acc' vs. '$attribs{__acc}'" if ($acc ne $attribs{__acc});
    my $name = "";
    if (exists $attribs{"sample name"}) {
      $name = $attribs{"sample name"};
    } elsif (exists $attribs{"Submitter Id"}) {
      $name = $attribs{"Submitter Id"};
    } elsif (exists $attribs{strain}) {
      $name = $attribs{strain};
    } elsif (exists $attribs{isolate}) {
      $name = $attribs{isolate};
    } elsif (exists $attribs{title}) {
      $name = $attribs{title};
    } elsif (exists $attribs{"virus identifier"}) {
      $name = $attribs{"virus identifier"};
    } elsif (exists $attribs{__sampleName}) {
      $name = $attribs{__sampleName};
    }
    $name =~ s@^SARS-Co[Vv]-2/@@;
    $name =~ s@^hCo[Vv]-19/@@;
    $name =~ s@^[Hh]uman/@@;
    $name =~ s@^Severe acute respiratory syndrome coronavirus 2/@@;
    $name =~ s@^/North America/@@;
    my $date = "";
    if (exists $attribs{"receipt date"} && exists $attribs{"collection date"}) {
      # Use the longer of the two in case one is just "2020"
      if (length($attribs{"receipt date"}) > length($attribs{"collection date"})) {
        $date = $attribs{"receipt date"};
      } else {
        $date = $attribs{"collection date"};
      }
    } elsif (exists $attribs{"receipt date"}) {
      $date = $attribs{"receipt date"};
    } elsif (exists $attribs{"collection date"}) {
      $date = $attribs{"collection date"};
    }
    my $lab = "";
    if (exists $attribs{"collecting institution"}) {
      $lab = $attribs{"collecting institution"};
    } elsif (exists $attribs{"collected by"}) {
      $lab = $attribs{"collected by"};
    } elsif (exists $attribs{"INSDC center name"}) {
      $lab = $attribs{"INSDC center name"};
    }
    my $author = "";
    if (exists $attribs{"collector name"}) {
      $author = $attribs{"collector name"};
    }
    my $country = "";
    if (exists $attribs{"geographic location"}) {
      $country = $attribs{"geographic location"};
    }
    my $locale = "";
    if (exists $attribs{"geographic location (region and locality)"}) {
      $locale = $attribs{"geographic location (region and locality)"};
    } elsif ($country =~ m/(.*):\s*(.*)/) {
      ($country, $locale) = ($1, $2);
    }
    my $hostId = "";
    if (exists $attribs{"host subject id"}) {
      $hostId = $attribs{"host subject id"};
    }
    my $sraId = "";
    if (exists $attribs{__sra}) {
      $sraId = $attribs{__sra};
    }
    my $epiId = "";
    if (exists $attribs{gisaid_accession}) {
      $epiId = $attribs{gisaid_accession};
    } elsif (exists $attribs{gisaid_accession_id}) {
      $epiId = $attribs{gisaid_accession_id};
    } elsif (exists $attribs{gisaid}) {
      $epiId = $attribs{gisaid};
    } elsif (exists $attribs{"gisaid id"}) {
      $epiId = $attribs{"gisaid id"};
    } elsif (exists $attribs{"gisaid accession id"}) {
      $epiId = $attribs{"gisaid accession id"};
    } elsif (exists $attribs{subgroup}) {
      $epiId = $attribs{subgroup};
    } elsif (exists $attribs{__epi}) {
      $epiId = $attribs{__epi};
    }

    print join("\t", $gi, $acc, $name, $date, $lab, $author, $country, $locale,
               $hostId, $sraId, $epiId) ."\n";
    %attribs = ();
  } elsif (/^Accession: /) {
    die "Can't parse Accession line $.:\n$_\t";
  }
}
