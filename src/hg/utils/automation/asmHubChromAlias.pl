#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc != 1) {
  printf STDERR "usage: asmHubChromAlias.pl asmId > asmId.chromAlias.tab\n\n";
  printf STDERR "where asmId is something like: GCF_000001735.4_TAIR10.1\n";
  printf STDERR "Outputs a tab file suitable for processing with ixIxx to\n";
  printf STDERR "create an index file to use in an assembly hub.\n";
  printf STDERR "This command assumes it is in a work directory in the\n";
  printf STDERR "assembly hub: .../asmId/trackData/chromAlias/\n";
  printf STDERR "and .../asmId/downloads/ and .../asmId/sequence/ have\n";
  printf STDERR "been created and processed for the hub build.\n";
  exit 255;
}

my %ucscToRefSeq;	# key is UCSC sequence name, value is RefSeq name
my %ucscToGenbank;	# key is UCSC sequence name, value is GenBank name
my $ucscNames = 0;	# == 1 if sequence is UCSC names, == 0 if NCBI names
my $dupCount = 0;
my %dupToSequence;	# key is duplicate name, value is target sequence name
			# to manage duplicates coming in from assembly report
my $sequenceCount = 0;
my %sequenceSizes;	# key is sequence name, value is sequence size

my $asmId = shift;

my %aliasOut;	# key is source name, value is a hash pointer where
		# key is alias name and value is chrom name
	# typical source names: ucsc, refseq, genbank, ensembl, assembly, ncbi

# the argument is the name of the 'native' source name
# where 'native' are the names in the assembly itself, thus the
# assembly names.  The other sources are aliases to that.
sub showAlias($) {
  my ($native) = @_;
  # second line is the column headers
  printf "# %s", $native;
  foreach my $source (sort keys %aliasOut) {
    printf "\t%s", $source if ($source ne $native);
  }
  printf "\n";

  # need to account for all sequence names for all sources
  # create a list of sequence names for each source that does *not* have
  # an alias
  my %noAlias;	# key is source, value is a hash pointer with
		# a key of seqName value 1 for those seqNames without alias
  foreach my $source (sort keys %aliasOut) {
    next if ($source eq $native);	# don't need to do this for native
    my %seqDone;	# key is seqName found in alias, value is 1
    my $hashPtr = $aliasOut{$source};
    foreach my $alias (sort keys %$hashPtr) {
      $seqDone{$hashPtr->{$alias}} = 1;
    }
    foreach my $sequence (sort keys %sequenceSizes) {
      next if (defined($seqDone{$sequence}));
      if (!defined($noAlias{$source})) {
        my %h;
        $noAlias{$source} = \%h;
      }
      $hashPtr = $noAlias{$source};
      $hashPtr->{$sequence} = 1;
    }
  }	#	foreach my $source (sort keys %aliasOut)

  my %chromIndex;	# key is sequence seqName in assembly, value
                        # is a tab separated list of aliases, in order by source
  foreach my $source (sort keys %aliasOut) {
    next if ($source eq $native);
    my $hashPtr = $aliasOut{$source};
    foreach my $alias (sort keys %$hashPtr) {
      my $seqName = $hashPtr->{$alias};
      $alias = "" if ($alias eq "na");
      if (defined($chromIndex{$seqName})) {
        $chromIndex{$seqName} .= "\t" . $alias;
      } else {
        $chromIndex{$seqName} = $alias;
      }
    }
    # catch up for those sequence names with no alias in this source
    if (defined($noAlias{$source})) {
      $hashPtr = $noAlias{$source};
      foreach my $sequence (sort keys %sequenceSizes) {
        next if (!defined($hashPtr->{$sequence}));
        if (defined($chromIndex{$sequence})) {
          $chromIndex{$sequence} .= "\t" . "";
        } else {
          $chromIndex{$sequence} = "";
        }
      }
    }
  }
  foreach my $seqName (sort keys %chromIndex) {
    printf "%s\t%s\n", $seqName, $chromIndex{$seqName};
  }
}

#  given an alias and a sequence name, add to result or verify identical
#  to previous add
sub addAlias($$$) {
  my ($source, $alias, $sequence) = @_;
  if ($alias eq "na") {
    return;
  }
  if ($sequence eq "na") {
    return;
  }
  # do not need to add the sequence name itself
  return if ($alias eq $sequence);
  if (!defined($aliasOut{$source})) {
     my %h;	# hash: key: alias name, value 'native' chrom name
     $aliasOut{$source} = \%h;
#     printf STDERR "# creating aliasOut{'%s'}\n", $source;
  }
  my $hashPtr = $aliasOut{$source};
  # already done, verify it is equivalent to previous request
  if (defined($hashPtr->{$alias})) {
     if ($sequence ne $hashPtr->{$alias}) {
        printf STDERR "ERROR: additional alias '%s:%s' does not match previous '%s'\n", $alias, $sequence, $hashPtr->{$alias};
        exit 255;
     }
     return;
  }
  $hashPtr->{$alias} = $sequence;
  return;
}

# asmSource - is this a genbank or refseq assembly
my $asmSource = "genbank";
my $isRefSeq = 0; #       == 0 for Genbank assembly, == 1 for RefSeq assembly
if ($asmId =~ m/^GCF/) {
#  printf STDERR "# processing a RefSeq assembly\n";
  $isRefSeq = 1;
  $asmSource = "refseq";
} else {
#  printf STDERR "# processing a GenBank assembly\n";
}

my $twoBit = "../../$asmId.2bit";

open (FH, "twoBitInfo $twoBit stdout|") or die "can not twoBitInfo $twoBit stdout";
while (my $line = <FH>) {
  chomp $line;
  my ($name, $size) = split('\s+', $line);
  $sequenceSizes{$name} = $size;
  ++$sequenceCount;
}

close (FH);
# printf STDERR "# counted %d sequence names in the twoBit file\n", $sequenceCount;

my $nameCount = 0;
my %ncbiToUcsc;	# key is NCBI sequence name, value is 'chr' UCSC chromosome name
my %ucscToNcbi;	# key is 'chr' UCSC name, value is NCBI sequence name
open (FH, "cat ../../sequence/*.names|") or die "can not cat ../../sequence/*.names";
while (my $line = <FH>) {
  chomp $line;
  my ($ucscName, $seqName) = split('\s+', $line);
  $ncbiToUcsc{$seqName} = $ucscName;
  $ucscToNcbi{$ucscName} = $seqName;
  ++$nameCount;
  $ucscNames = 1 if (defined($sequenceSizes{$ucscName}));
  if ($isRefSeq) {
    $ucscToRefSeq{$ucscName} = $seqName;
  } else {
    $ucscToGenbank{$ucscName} = $seqName;
  }
}
close (FH);

my $dupsNotFound = 0;
my $dupsList = "../../download/$asmId.dups.txt.gz";
if ( -s "$dupsList" ) {
  open (FH, "zcat $dupsList | awk '{print \$1, \$3}'|") or die "can not read $dupsList";
  while (my $line = <FH>) {
    chomp $line;
    my ($dupAlias, $dupTarget) = split('\s+', $line);
    $dupToSequence{$dupAlias} = $dupTarget;
    if ($ucscNames) {
      if (!defined($ncbiToUcsc{$dupTarget})) {
       printf STDERR "# ERROR: can not find dupTarget: $dupTarget in ncbiToUcsc for dupAlias: $dupAlias\n";
         $dupsNotFound += 1;
      } else {
        printf STDERR "# addAlias(\"ucsc\", $dupAlias, $ncbiToUcsc{$dupTarget});\n";
      }
    } else {
        printf STDERR "# addAlias($asmSource, $dupAlias, $dupTarget);\n";
    }
    printf STDERR "# adding duplicate alias %s\n", $dupAlias;
    ++$dupCount;
  }
  close (FH);
}

if ($dupsNotFound > 0) {
  printf STDERR "ERROR: can not find %d duplicate names\n", $dupsNotFound;
  exit 255;
}
if ($dupCount > 0) {
  printf STDERR "# processed %d duplicates\n", $dupCount;
}

# when not a UCSC named assembly, add the UCSC names as aliases
if (! $ucscNames) {
  if ($isRefSeq) {
    foreach my $ucscName (sort keys %ucscToRefSeq) {
       if (defined($dupToSequence{$ucscToRefSeq{$ucscName}})) {   # avoid duplicates
         printf STDERR "# skipping duplicate name $ucscToRefSeq{$ucscName}\n";
       } else {
         addAlias("ucsc", $ucscName, $ucscToRefSeq{$ucscName});
       }
    }
  } else {
    foreach my $ucscName (sort keys %ucscToGenbank) {
       if (defined($dupToSequence{$ucscToGenbank{$ucscName}})) {   # avoid duplicates
         printf STDERR "# skipping duplicate name $ucscToGenbank{$ucscName}\n";
       } else {
         addAlias("ucsc", $ucscName, $ucscToGenbank{$ucscName});
       }
    }
  }
}


if ($sequenceCount != $nameCount) {
  printf STDERR "ERROR: do not find the same name count in sequence vs. names files\n";
  printf STDERR "sequenceCount %d != %d names count - %d duplicates\n", $sequenceCount, $nameCount, $dupCount;
  exit 255;
}

# printf STDERR "# initial set of %d UCSC names\n", $sequenceCount;
### first set of names is the UCSC to NCBI sequence names
foreach my $sequence (sort keys %sequenceSizes) {
  my $seqName = $sequence;
  if (defined($dupToSequence{$seqName})) {   # avoid duplicates
    printf STDERR "# skipping duplicate name $seqName\n";
    next;
  }
  my $alias = $sequence;
  if ($ucscNames) {
     $alias = $ucscToNcbi{$seqName};
  } else {
     $alias = $ncbiToUcsc{$seqName};
  } 
  if (!defined($alias)) {
    printf STDERR "ERROR: can not find alias name for '%s' sequence\n", $seqName;
    exit 255;
  }
  addAlias($ucscNames ? "ucsc" : $asmSource, $alias, $seqName);
}

# foreach my $sequence (sort keys %chrNames) {
#   printf "%s\t%s\n",  $chrNames{$sequence}, $sequence;
# }

### next set of names are the equivalents declared by NCBI
### if they exist
my %chr2acc;	# key is sequence name, value is NCBI chromosome name
my $asmStructCount = `ls  ../../download/${asmId}_assembly_structure/*/*/chr2acc 2> /dev/null | wc -l`;
chomp $asmStructCount;
if ( $asmStructCount > 0 ) {
#  printf STDERR "# second name set processing chr2acc files\n";
  open (FH, "grep -h -v '^#' ../../download/${asmId}_assembly_structure/*/*/chr2acc|") or die "can not grep chr2acc files";
  while (my $line = <FH>) {
    chomp $line;
    my ($alias, $seqName) = split('\t', $line);
    $alias =~ s/ /_/g;	# some assemblies have spaces in chr names ...
    $alias =~ s/:/_/g;	# one assembly had : in a chr name
    $chr2acc{$seqName} = $alias;
    if ($ucscNames) {
       $seqName = $ncbiToUcsc{$seqName};
    }
    if (defined($dupToSequence{$seqName})) {   # avoid duplicates
      printf STDERR "# skipping duplicate name $seqName\n";
    } else {
      addAlias("ncbi", $alias, $seqName);
    }
  }
  close (FH);
}

# foreach my $sequence (sort keys %chr2acc) {
#   printf "%s\t%s\n",  $chr2acc{$sequence}, $sequence;
# }

my $dbgCount = 0;
# printf STDERR "# third set processing assembly_report\n";
# column 1 is the 'assembly' name
# column 5 is the GenBank-Accn, column 7 is the RefSeq-Accn
open (FH, "grep -v '^#' ../../download/${asmId}_assembly_report.txt | cut -d\$'\t' -f1,5,7|") or die "can not grep assembly_report";
while (my $line = <FH>) {
  chomp $line;
  ++$dbgCount;
  my ($asmName, $gbkName, $refSeqName) = split('\s+', $line);
  if (defined($dupToSequence{$asmName})) {   # avoid duplicates
     printf STDERR "# skipping duplicate name $asmName\n";
     next;
  } elsif (defined($dupToSequence{$gbkName})) {   # avoid duplicates
     printf STDERR "# skipping duplicate name $gbkName\n";
     next;
  } elsif (defined($dupToSequence{$refSeqName})) {   # avoid duplicates
     printf STDERR "# skipping duplicate name $refSeqName\n";
     next;
  }
  printf STDERR "# '%s'\t'%s'\t'%s'\n", $asmName, $gbkName, $refSeqName if ($dbgCount < 5);
#  next if ($refSeqName eq "na");	# may not be any RefSeq name
#  next if ($gbkName eq "na");	# may not be any GenBank name
  if ($refSeqName ne "na") {
    my $seqName = $refSeqName;
    if (! $isRefSeq) {
      $seqName = $gbkName;
    }
    if ($ucscNames) {
       $seqName = $ncbiToUcsc{$seqName};
    }
    if (!defined($seqName)) {
       if (defined($aliasOut{"refseq"})) {
         if (defined($aliasOut{"refseq"}->{$refSeqName})) {
            $seqName = $aliasOut{"refseq"}->{$refSeqName};
         }
       }
    }
    if (defined($seqName)) {
      if (defined($dupToSequence{$seqName})) {
        if (defined($dupToSequence{$seqName})) {   # avoid duplicates
          printf STDERR "# skipping duplicate name $dupToSequence{$seqName}\n";
        } else {
          addAlias("refseq", $refSeqName, $dupToSequence{$seqName});
          addAlias("assembly", $asmName, $dupToSequence{$seqName});
        }
      } else {
        if (defined($dupToSequence{$seqName})) {   # avoid duplicates
          printf STDERR "# skipping duplicate name $seqName\n";
        } else {
          addAlias("refseq", $refSeqName, $seqName);
          addAlias("assembly", $asmName, $seqName);
        }
      }
    }
  }	#	if ($refSeqName ne "na")

  if ($gbkName ne "na") {
    my $seqName = $gbkName;
    if ($isRefSeq) {
      $seqName = $refSeqName;
    }
    if ($ucscNames) {
       $seqName = $ncbiToUcsc{$seqName};
    }
    if (!defined($seqName)) {
       if (defined($aliasOut{"genbank"})) {
         if (defined($aliasOut{"genbank"}->{$gbkName})) {
            $seqName = $aliasOut{"genbank"}->{$gbkName};
         }
       }
    }
    if (defined($seqName)) {
      if (defined($dupToSequence{$seqName})) {
        addAlias("genbank", $gbkName, $dupToSequence{$seqName});
        addAlias("assembly", $asmName, $dupToSequence{$seqName});
      } else {
        addAlias("genbank", $gbkName, $seqName);
        addAlias("assembly", $asmName, $seqName);
      }
    }
  }	#	if ($gbkName ne "na")
}	#	done reading assembly_report.txt
close (FH);

showAlias($ucscNames ? "ucsc" : $asmSource);
