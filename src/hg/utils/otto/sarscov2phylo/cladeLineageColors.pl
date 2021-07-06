#!/usr/bin/env perl

# Read in a file with 4 columns: sample name, Nextstrain clade, Pangolin lineage, GISAID clade
# Choose a color for the sample according to the scheme in Figure 1 of
# https://www.eurosurveillance.org/content/10.2807/1560-7917.ES.2020.25.32.2001410
# Make 3 output files: nextstrainColors, lineageColors, gisaidColors

use warnings;
use strict;

my %lin2col = ( # Some, but not all, of the "gray area" where GH and Nextstrain 20A overlap
               "B.1.22" => "#888888",
               "B.1.37" => "#888888",
               "B.1.13" => "#888888",
               "B.1.36" => "#888888",
               "B.1.9"  => "#888888",

               # Some, but not all, of the green B.1 sublineages (G and 20A)
               "B.1.6"  => "#44cc44",
               "B.1.67" => "#88cc88",
               "B.1.5"  => "#44cc44",
               "B.1.72" => "#88cc88",
               "B.1.70" => "#44cc44",
               "B.1.8"  => "#88cc88",
               "B.1.34" => "#44cc44",
               "B.1.71" => "#88cc88",
               "B.1.69" => "#44cc44",
               "B.1.19" => "#88cc88",
               "B.1.23" => "#44cc44",
               "B.1.32" => "#88cc88",
               "B.1.33" => "#44cc44",
               "B.1.35" => "#88cc88",

               # Some, but not all, of the blue B.1 sublineages (GH and 20C)
               "B.1.12" => "#8888ff",
               "B.1.30" => "#8888ff",
               "B.1.38" => "#8888ff",
               "B.1.66" => "#8888ff",
               "B.1.44" => "#8888ff",
               "B.1.40" => "#8888ff",
               "B.1.31" => "#8888ff",
               "B.1.39" => "#8888ff",
               "B.1.41" => "#8888ff",
               "B.1.3"  => "#8888ff",
               "B.1.26" => "#8888ff",
               "B.1.43" => "#8888ff",
               "B.1.29" => "#8888ff"
              );

my %ns2col = ( # Nextstrain clade coloring
              "19B" => "#ec676d",
              "19A" => "#f79e43",
              "20A" => "#b6d77a",
              "20C" => "#8fd4ed",
              "20B" => "#a692c3",
              # Added in Jan. 2021, when Nextstrain added a bunch of new clades:
              "20D" => "#8020a0", # from 20B (purple)
              "20E" => "#44cc44", # from 20A (green)
              "20F" => "#8822aa", # from 20B (purple)
              "20G" => "#8888ff", # from 20C (blue)
              "20H" => "#6666ff", # from 20C (blue)
              "20I" => "#cc44ee", # from 20B (purple)
             );

my %gc2col = ( # GISAID clade coloring
              "S"  => "#ec676d",
              "L"  => "#f79e43",
              "O"  => "#f9d136",
              "V"  => "#faea95",
              "G"  => "#b6d77a",
              "GH" => "#8fd4ed",
              "GR" => "#a692c3",
              # Added after the Eurosurveillance paper in a gray area -- make it dark gray:
              "GV" => "#666666"
             );

open(my $nsF, ">nextstrainColors") || die "Can't open nextstrainColors: $!";
open(my $linF, ">lineageColors") || die "Can't open lineageColors: $!";
open(my $gF, ">gisaidColors") || die "Can't open gisaidColors: $!";

while (<>) {
  chomp;
  my ($sample, $nsClade, $lineage, $gClade) = split("\t");
  # Strip subclade and other naming fluff from Nextstrain clade
  $nsClade =~ s/[\. \/].*//;
  if (defined $ns2col{$nsClade}) {
    print $nsF "$sample\t$ns2col{$nsClade}\n";
  } else {
    print $nsF "$sample\t#000000\n";
  }
  if ($gClade && defined $gc2col{$gClade}) {
    print $gF "$sample\t$gc2col{$gClade}\n";
  } else {
    print $gF "$sample\t#000000\n";
  }
  # There are too many lineages to keep track of.  Use Nextstrain and GISAID clades to help
  # pick colors in some cases.  Use darker and lighter shades to help differentiate some of
  # the lineages within color categories.
  my $color = "#000000";
  if (defined $lin2col{$lineage}) {
    $color = $lin2col{$lineage};
  } elsif ($lineage eq "") {
    # No lineage assigned; fall back on clade.
    if (defined $ns2col{$nsClade} && $gc2col{$gClade}) {
      if ($nsClade eq "20A" && $gClade eq "GH") {
        $color = "#888888";
      } elsif ($nsClade eq "20A" && $gClade eq "GV") {
        $color = "#666666";
      } else {
        $color = $gc2col{$gClade};
      }
    } elsif (defined $gc2col{$gClade}) {
      $color = $gc2col{$gClade};
    } elsif (defined $ns2col{$nsClade}) {
      $color = $ns2col{$nsClade};
    }
  } else {
    my @linPath = split(/\./, $lineage);
    my $linDepth = scalar @linPath;
    if (! $linPath[0]) {
      die "no linpath[0] for '$lineage'\n";
    } elsif ($linPath[0] eq "C") {
      $linDepth += 4;
    } elsif ($linPath[0] eq "D") {
      $linDepth += 5;
    }
    if ($lineage eq "B.1.1" ||
        substr($lineage, 0, 6) eq "B.1.1." ||
        substr($lineage, 0, 1) =~ /[C-Z]/) {
      # Shades of purple -- minimum depth is 3, use even/odd to alternate colors
      my $endNum = ($lineage =~ m/\.(\d+)$/) ? ($1 + 0) : 0;
      if ($linDepth > 5) {
        $color = ($endNum & 1) ? "#602080" : "#8020a0";
      } elsif ($linDepth > 4) {
        $color = ($endNum & 1) ? "#662288" : "#8822aa";
      } elsif ($linDepth > 3) {
        $color = ($endNum & 1) ? "#aa44cc" : "#cc44ee";
      } else {
        $color = "#cc88ff";
      }
    } elsif ($lineage eq "B.1" ||
             substr($lineage, 0, 4) eq "B.1.") {
      # Green, blue or gray... complicated.  We really need a tree-based coloring method.
      my $endNum = ($lineage =~ m/\.(\d+)$/) ? ($1 + 0) : 0;
      if (substr($lineage, 0, 6) eq "B.1.5.") {
        $color = ($endNum & 1) ? "#88dd88" : "#88ee88";
      } else {
        if ($nsClade eq "20A" && $gClade eq "GH") {
          # gray
          $color = ($endNum & 1) ? "#aaaaaa" : "#cccccc";
        } elsif ($nsClade eq "20A" && $gClade eq "GV") {
          $color = ($endNum & 1) ? "#777777" : "#888888";
        } elsif ($gClade eq "G") {
          # green
          $color = ($endNum & 1) ? "#66cc66" : "#88cc88";
        } elsif ($gClade eq "GH") {
          # blue
          $color = ($endNum & 1) ? "#6666ff" : "#8888ff";
        } else {
          # gray
          $color = ($endNum & 1) ? "#aaaaaa" : "#cccccc";
        }
      }
    } elsif (substr($lineage, 0, 1) eq "B") {
      # Shades of orange (not as much depth in B.>1)
      my $endNum = ($lineage =~ m/\.(\d+)$/) ? ($1 + 0) : 0;
      if ($linDepth >= 3) {
        $color = ($endNum & 1) ? "#ee6640" : "#ff774a";
      } elsif ($linDepth == 2) {
        $color = ($endNum & 1) ? "#f07850" : "#ff8844";
      } else {
        $color = "#ff9060";
      }
    } elsif (substr($lineage, 0, 1) eq "A") {
      # Shades of red
      my $endNum = ($lineage =~ m/\.(\d+)$/) ? ($1 + 0) : 0;
      if ($linDepth >= 3) {
        $color = ($endNum & 1) ? "#bb2222" : "#cc2222";
      } elsif ($linDepth == 2) {
        $color = ($endNum & 1) ? "#dd2222" : "#ee3333";
      } else {
        $color = "#ff6666";
      }
    }
  }
  print $linF "$sample\t$color\n";
}
