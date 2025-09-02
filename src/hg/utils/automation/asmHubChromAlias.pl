#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc != 2) {
  printf STDERR "usage: asmHubChromAlias.pl asmId ncbiAsmId > asmId.chromAlias.tab\n\n";
  printf STDERR "where asmId is something like: GCF_000001735.4_TAIR10.1\n";
  printf STDERR "Outputs a tab file suitable for processing with ixIxx to\n";
  printf STDERR "create an index file to use in an assembly hub.\n";
  printf STDERR "This command assumes it is in a work directory in the\n";
  printf STDERR "assembly hub: .../asmId/trackData/chromAlias/\n";
  printf STDERR "and ../../download/ and ../../sequence/ have\n";
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
my $ncbiAsmId = shift;

my %aliasOut;	# key is source name, value is a hash pointer where
		# key is alias name and value is chrom name
	# typical source names: ucsc, refseq, genbank, ensembl, assembly, ncbi
my %aliasDone;	# key is source name, value is hash pointer where
		# key is chrom name value is alias, to check
                # for duplicate incoming

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
}	#	sub showAlias($)

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
  # it is OK to allow duplicate names, different naming authorities could
  # have the same name, found for example in GCF_006542625.1_Asia_NLE_v1
  # which has UCSC names identical to 'assembly' names and the hub has been
  # build with UCSC names
  if (!defined($aliasOut{$source})) {
     my %h;	# hash: key: alias name, value 'native' chrom name
     $aliasOut{$source} = \%h;
     my %t;	# hash: key: native sequence name, value alias
     $aliasDone{$source} = \%t;
  }
  my $hashPtr = $aliasOut{$source};
  my $donePtr = $aliasDone{$source};
  # already done, verify it is equivalent to previous request
  if (defined($hashPtr->{$alias})) {
     if ($sequence ne $hashPtr->{$alias}) {
        printf STDERR "ERROR: additional alias '%s:%s' does not match previous '%s'\n", $alias, $sequence, $hashPtr->{$alias};
        exit 255;
     }
     return;
  }
  if (defined($donePtr->{$sequence})) {
    printf STDERR "# WARNING multiple inputs for source '%s.%s' was '%s' new '%s'\n", $source, $sequence, $donePtr->{$sequence}, $alias;
  } else {
    $donePtr->{$sequence} = $alias;
    $hashPtr->{$alias} = $sequence;
  }
  return;
}	#	sub addAlias($$$)

# asmSource - is this a genbank or refseq assembly
my $asmSource = "genbank";
my $isRefSeq = 0; #       == 0 for Genbank assembly, == 1 for RefSeq assembly
if ($ncbiAsmId =~ m/^GCF/) {
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

### improvement to customName system, now allow multiple name schemes to
### be defined in the single customNames.tsv file.  Must have a first
### line to name the columns.  Will tolerate previous scheme of only one
### column of names with no title line.

my $customNameCount = 0;
my @customSource;	# when first line present with source names 

if ( -s "customNames.tsv" ) {
  open (FH, "<customNames.tsv") or die "can not read customNames.tsv";
  while (my $line = <FH>) {
    chomp $line;
    if ( (0 == $customNameCount) && ($line =~ m/^#/) ) {
       my @sources = split('\t', $line);
       for (my $i = 1; $i < scalar(@sources); ++$i) {
          push @customSource, $sources[$i];
       }
       next;
    }

    my @aliases = split('\s+', $line);
    if ( (scalar(@aliases) > 2) && !defined($customSource[0]) ) {
       printf STDERR "ERROR: multiple name scheme found in customNames.txt\n";
       printf STDERR "       but missing name scheme title line\n";
       printf STDERR "Should have a first line starting with '# native'\n";
       printf STDERR "and then tab separated list of names to specify the\n";
       printf STDERR "the title name of the that column of names\n";
       exit 255;
    }
    my $native = $aliases[0];
    if (!defined($sequenceSizes{$native})) {
       printf STDERR "ERROR: processing customNames.tsv given native name\n";
       printf STDERR " '%s' that does not exist\n", $native;
       exit 255;
    }
    for (my $i = 1; $i < scalar(@aliases); ++$i) {
      my $alias = $aliases[$i];
      if (defined($customSource[$i-1])) {
         addAlias($customSource[$i-1], $alias, $native);
      } else {
         addAlias("custom", $alias, $native);
      }
    }
    ++$customNameCount;
  }
  close (FH);
  if (defined($customSource[0])) {
    printf STDERR "# read %d custom alias names from customNames.tsv with %d naming schemes defined\n", $customNameCount, scalar(@customSource);
  } else {
    printf STDERR "# read %d custom alias names from customNames.tsv\n", $customNameCount;
  }
}

my %ensemblName;	# key is native sequence name, value is a ensembl alias
my $ensemblNameCount = 0;

if ( -s "ensemblNames.tsv" ) {
  open (FH, "<ensemblNames.tsv") or die "can not read ensemblNames.tsv";
  while (my $line = <FH>) {
    chomp $line;
    my ($native, $alias) = split('\s+', $line);
    if (!defined($sequenceSizes{$native})) {
       printf STDERR "ERROR: processing ensemblNames.tsv given native name\n";
       printf STDERR " '%s' that does not exist (alias: %s)\n", $native, $alias;
       exit 255;
    }
    $ensemblName{$native} = $alias;
    ++$ensemblNameCount;
    addAlias("ensembl", $alias, $native);
  }
  close (FH);
  printf STDERR "# read %d ensembl alias names from ensemblNames.tsv\n", $ensemblNameCount;
}

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

# with the new style system, can not have multiple aliases for
# the same sequence name.  The older system did do this.
# Need to record them here so they can be skipped later.
my $dupsNotFound = 0;
my $dupsList = `ls ../../download/*.dups.txt.gz 2> /dev/null|head -1`;
chomp $dupsList if (defined($dupsList));
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
#      } else {
#        printf STDERR "# dupList addAlias(\"ucsc\", $dupAlias, $ncbiToUcsc{$dupTarget});\n";
      }
#    } else {
#        printf STDERR "# dupList addAlias($asmSource, $dupAlias, $dupTarget);\n";
    }
    printf STDERR "# would have added duplicate alias %s for %s\n", $dupAlias, $dupTarget;
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
my $asmStructCount = `ls ../../download/*_assembly_structure/*/*/chr2acc 2> /dev/null | wc -l`;
chomp $asmStructCount;
if ( $asmStructCount > 0 ) {
#  printf STDERR "# second name set processing chr2acc files\n";
  open (FH, "grep -h -v '^#' ../../download/*_assembly_structure/*/*/chr2acc|") or die "can not grep chr2acc files";
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
open (FH, "grep -v '^#' ../../download/*_assembly_report.txt | cut -d\$'\t' -f1,5,7|") or die "can not grep assembly_report";
while (my $line = <FH>) {
  chomp $line;
  ++$dbgCount;
  my ($asmName, $gbkName, $refSeqName) = split('\t', $line);
  $asmName =~ s/ /_/g;	# some assemblies have spaces in chr names ...
  $asmName =~ s/:/_/g;	# one assembly had : in chr name
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
#  next if ($refSeqName eq "na");	# may not be any RefSeq name
#  next if ($gbkName eq "na");	# may not be any GenBank name
  # fill in ncbiToUcsc for potentially the 'other' NCBI name
  if ($refSeqName ne "na"  && $gbkName ne "na") {
    if (defined($ncbiToUcsc{$refSeqName}) && !defined($ncbiToUcsc{$gbkName})) {
      $ncbiToUcsc{$gbkName} = $ncbiToUcsc{$refSeqName};
      $ucscToNcbi{$ncbiToUcsc{$refSeqName}} = $gbkName;
      }
    if (defined($ncbiToUcsc{$gbkName}) && !defined($ncbiToUcsc{$refSeqName})) {
      $ncbiToUcsc{$refSeqName} = $ncbiToUcsc{$gbkName};
      $ucscToNcbi{$ncbiToUcsc{$gbkName}} = $refSeqName;
    }
  }
  if (defined($ncbiToUcsc{$gbkName})) {
     printf STDERR "# asmRpt: '%s'\t'%s'\t'%s'\t'%s'\n", $asmName, $gbkName, $refSeqName, $ncbiToUcsc{$gbkName} if ($dbgCount < 5);
  } elsif (defined($ncbiToUcsc{$refSeqName})) {
     printf STDERR "# asmRpt: '%s'\t'%s'\t'%s'\t'%s'\n", $asmName, $gbkName, $refSeqName, $ncbiToUcsc{$refSeqName} if ($dbgCount < 5);
  } else {
     printf STDERR "# asmRpt: '%s'\t'%s'\t'%s'\tno UCSC name\n", $asmName, $gbkName, $refSeqName if ($dbgCount < 5);
  }
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
