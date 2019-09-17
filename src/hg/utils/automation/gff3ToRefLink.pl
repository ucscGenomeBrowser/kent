#!/usr/bin/env perl

use strict;
use warnings;

sub usage {
  printf STDERR "usage: gff3ToRefLink.pl [raFile.txt] [ncbi.gff.gz] [pragmaLabels.txt] > refLink.tab\n";
  printf STDERR "- raFile.txt is the output of gbProcess \${asmId}_rna.gbff.gz\n";
  printf STDERR "- ncbi.gff.gz is \${asmId}_genomic.gff.gz\n";
  printf STDERR "- pragmaLabels.txt has words that appear before '::' within ##*-START/##*-END\n";
  printf STDERR "  text in \${asmId}_rna.gbff.gz, one label per line.\n";
  printf STDERR "output to stdout is the tab separated ncbiRefSeqLink data for each item\n";
  printf STDERR "This is intended to be called by doNcbiRefSeq.pl.\n";
  exit 255;
} # usage

# this order list of items will keep the tab output consistent so all
#  'columns' of the data in the tab output will be the same order always.
# there are two columns before these, the id (transcript_id) and the gene reviewed 'status',
# and two columns after these items, the 'description' string from gbProcess
# and the externalId for external URL outlinks
my @tabOrderOutput = ( 'gene', 'product', 'transcript_id', 'protein_id', 'geneid',
   'mim', 'hgnc', 'genbank', 'pseudo', 'gbkey', 'source',
   'gene_biotype', 'gene_synonym', 'ncrna_class', 'note' );

# raFile.txt has 3-letter abbreviated status (or no status given for model-only X[MR]_*)
my %rssToStatus = ( inf => 'Inferred',
                    ''  => 'Model',
                    pre => 'Predicted',
                    pro => 'Provisional',
                    rev => 'Reviewed',
                    val => 'Validated' );

# Lowercased GFF tags that we will make use of, ignore others:
my %tagsOfInterest = ( id => 1,
                       parent => 1,
                       dbxref => 1,
                       name => 1,
                       gene => 1,
                       product => 1,
                       transcript_id => 1,
                       protein_id => 1,
                       pseudo => 1,
                       gbkey => 1,
                       gene_biotype => 1,
                       gene_synonym => 1,
                       ncrna_class => 1,   # Note: this doesn't appear in recent GFF as of 2018
                       note => 1,
                     );

# Lowercased tags to extract from Dbxref tag's list value:
my @externalDbs = qw/mgi bgd rgd sgd zfin flybase wormbase xenbase/;
my %dbxrefExternalId = map { $_ => 1 } @externalDbs;

sub mustOpen ($) {
  # Open possibly gzipped file or "stdin" for reading, return filehandle or die with error.
  my ($fileName) = @_;
  my $fh;
  if ($fileName =~ m/.gz$/) {
    open ($fh, "zcat $fileName |") or die "can not read $fileName: $!";
  } elsif ($fileName =~ m/^stdin$/) {
    open ($fh, "</dev/stdin") or die "can not read $fileName: $!";
  } else {
    open ($fh, "<$fileName") or die "can not read $fileName: $!";
  }
  return $fh;
} # mustOpen

sub parseGff3($) {
# Parse GFF3 into a tree structure using parent pointers.  Store only tags of interest.
# Children store only tags that are not present or differ from the parent's tags.
  my ($gffFile) = @_;
  my %gff;
  my @topLevelIds;
  my $fh = mustOpen($gffFile);
  while (my $line = <$fh>) {
    chomp $line;
    next if ($line =~ m/^#/);
    my @a = split('\t', $line);
    my ($source, $type, $attributes) = ($a[1], $a[2], $a[8]);
    next if ($type eq 'region' || $type eq 'cDNA_match' || $type eq 'match' ||
             $type eq 'sequence_feature');
    my %tags;
    foreach my $tagEqVal (split(';', $attributes)) {
      if ($tagEqVal =~ m/^(\w+)=(.*)/) {
        my ($tag, $val) = (lc($1), $2);
        if ($tagsOfInterest{$tag}) {
          $tags{$tag} = $val;
        }
      } else {
        die "parse error: expecting tag=val, got '$tagEqVal'";
      }
    }
    die "ID not found" if (! exists($tags{id}));
    my $id = $tags{id};
    if (exists($gff{$id})) {
      # So weird that NCBI's GFF has unique IDs for exons but not CDS.
      # And non-uniq IDs for a few sequence_feature, even primary_transcript mir-8199 in ce11...
      warn "Encountered a second record with ID=$id" unless ($id =~ /^cds/);
      next;
    }
    $gff{$id} = \%tags;
    # Add source and type columns as pseudo-tags -- make sure there aren't actual tags w/those names.
    die "Tag name conflict for 'type'." if (exists($tags{type}));
    $tags{type} = $type;
    die "Tag name conflict for 'source'." if (exists($tags{source}));
    $tags{source} = $source;
    # If this has a parent, hook up parent to this child; otherwise, this is a top-level element.
    if (exists($tags{parent})) {
      my $parentId = $tags{parent};
      # We're counting on NCBI's GFF to define parents before children.
      if (! exists($gff{$parentId})) {
        die "ID $id: Parent ID $parentId has not already been defined in GFF.";
      }
      push @{$gff{$parentId}->{children}}, $id;
    } else {
      # No parent -- this is top level.
      push @topLevelIds, $id;
    }
  }
  close($fh);
  print STDERR "Processed " . scalar(keys %gff) . " elements.\n";
  print STDERR "Found " . scalar(@topLevelIds) . " top level IDs.\n";
  # print STDERR join(", ", @topLevelIds) . "\n";
  return \%gff, \@topLevelIds;
} # parseGff3

sub parseDbXref($) {
  # If $txTags has dbxref, parse it out into components and add the relevant ones as new tags.
  my ($txTags) = @_;
  my $dbXref = $txTags->{dbxref};
  if ($dbXref) {
    foreach my $xref (split(',', $dbXref)) {
      if ($xref =~ m/^([\w\/_-]+):(.*)/) {
        # Most are tag:value, some are tag:tag:value
        my ($tag, $value) = (lc($1), $2);
        if ($tag eq 'hgnc') {
          $value =~ s/^HGNC://;
          $txTags->{$tag} = $value;
        } elsif ($tag eq 'geneid' || $tag eq 'genbank' || $tag eq 'mim' ||
                 $dbxrefExternalId{$tag}) {
          $txTags->{$tag} = $value;
        }
      } else {
        warn "unexpected format of Dbxref item '$xref' (in '$dbXref')\n";
      }
    }
    delete $txTags->{dbxref};
  }
} # parseDbXref

sub mergeOutTagsOne($$;$) {
# Merge in tag data from $tags to $outTags, which may already have some values
# (hopefully not conflicting).
  my ($gff, $tags, $outTags) = @_;
  if (! defined $outTags) {
    $outTags = {};
  }
  parseDbXref($tags);
  my $name = $tags->{name};
  $name = $tags->{id} if (! $name);
  foreach my $outTag (@tabOrderOutput, @externalDbs) {
    my $newVal = $tags->{$outTag};
    if (defined $newVal) {
      if (exists $outTags->{$outTag} ) {
        # Check for conflicts
        my $oldVal = $outTags->{$outTag};
        if ($oldVal ne $newVal) {
          if ($outTag eq 'note') {
            # Note= can vary between different mappings of the same NM, and from mRNA to exon/CDS.
            if (($newVal =~ m/^The RefSeq transcript has .* compared to this genomic sequence/ ||
                 $newVal =~ m/^Mappings of the RefSeq transcript to different genomic/) &&
                ($oldVal =~ m/^The RefSeq transcript has .* compared to this genomic sequence/ ||
                 $oldVal =~ m/^Mappings of the RefSeq transcript to different genomic/)) {
              $outTags->{$outTag} = "Mappings of the RefSeq transcript to different genomic " .
                "sequences have varying alignment differences";
            } elsif ($newVal =~ m/^The RefSeq protein / ||
                     $newVal =~ m/ is encoded by transcript variant / ||
                     $newVal =~ m/^The RefSeq transcript / ||
                     $newVal =~ m/^The sequence of the model RefSeq /) {
              # ignore mapping-specific details.
            } elsif ($newVal =~ m/^(.*translation initiation codon)/) {
              # This sometimes appears in CDS records; I think it's interesting enough to add.
              $outTags->{$outTag} = "$oldVal%3B$newVal";
            } else {
              warn "Conflict for $name tag $outTag: '" . $oldVal . "' vs. '" .
                $newVal . "'\n";
            }
          } elsif ($outTag eq 'gene_synonym') {
            # The list can expand from original mappings to new mappings on patches...
            if (length($newVal) > length($oldVal)) {
              $outTags->{$outTag} = $newVal;
            }
          } elsif ($outTag eq 'gene') {
            # Gene symbol can also be updated in patch updates but not fixed in older records.
            warn "Gene symbol for $name updated? $oldVal --> $newVal\n";
            $outTags->{$outTag} = $newVal;
          } elsif ($outTag eq 'source' &&
                  (($newVal eq 'Curated Genomic' && $oldVal eq 'BestRefSeq') ||
                   ($newVal eq 'BestRefSeq' && $oldVal eq 'Curated Genomic') ||
                   (index($newVal, $oldVal) >= 0))) {
            # ignore.  Sometimes it's one on some contig and the other on some other contig.
            # Gene source can be list of transcript sources e.g. "BestRefSeq%2CGnomon"
          } elsif ($outTag ne 'name' && $outTag ne 'genbank' && $outTag ne 'gbkey' &&
                   $outTag ne 'product') {
            warn "Conflict for $name tag $outTag: '" . $oldVal . "' vs. '" .
              $newVal . "'\n";
          }
        }
      } else {
        $outTags->{$outTag} = $newVal;
      }
    }
  }
  return $outTags;
} # mergeOutTagsOne

sub mergeOutTags($$;$) {
# Build up values of output columns by merging in tag data from $tags and descendents
# to $outTags, which may already have some values (hopefully not conflicting).
  my ($gff, $tags, $outTags) = @_;
  if (! defined $outTags) {
    $outTags = {};
  }
  mergeOutTagsOne($gff, $tags, $outTags);
  foreach my $kidId (@{$tags->{children}}) {
    my $kidTags = $gff->{$kidId};
    &mergeOutTags($gff, $kidTags, $outTags);
  }
  return $outTags;
} # mergeOutTags

sub collectColumns($$) {
# Find [NXY][MRP]_ transcript records under @topLevelIds (usually children of type=gene top-level)
# and output ncbiRefSeqLink lines for each.
  my ($gff, $topLevelIds) = @_;
  my %linkOut;
  foreach my $topId (@{$topLevelIds}) {
    die "top level ID $topId not found" if (!exists($gff->{$topId}));
    my $geneTags = $gff->{$topId};
    my $geneName = $geneTags->{name} || "";
    if ($geneName =~ /^[NXY][MRP]_\d+/) {
      die "top level ID $topId looks NM-ish ($geneName)";
    }
    next unless exists($geneTags->{children});
    my $topType = $geneTags->{type};
    if ($topType eq 'gene' || $topType eq 'pseudogene') {
      foreach my $txId (@{$geneTags->{children}}) {
        die "top level ID $topId child $txId not found" if (!exists($gff->{$txId}));
        my $txTags = $gff->{$txId};
        my $txName = $txTags->{name};
        next unless defined $txName;
        if ($txName !~ /^[NXY][MRP]_\d+/) {
          if ($topType ne 'pseudogene') {
            warn "top level ID $topId child $txId has non-acc name '$txName'";
          }
          next;
        }
        $linkOut{$txName} = mergeOutTags($gff, $txTags, $linkOut{$txName});
        # Add top-level-only tags but don't recurse to cousins:
        $linkOut{$txName} = mergeOutTagsOne($gff, $geneTags, $linkOut{$txName});
        # There are some cases in GCF_000146045.2_R64_genomic.gff.gz where the mRNA and gene
        # records have no gene=... -- but the gene record still has Name=... with the gene symbol.
        if (! $linkOut{$txName}->{gene} && $geneTags->{name}) {
          $linkOut{$txName}->{gene} = $geneTags->{name};
        }
      }
    } elsif ($topType ne 'rRNA' && $topType ne 'tRNA') {
      warn "top level ID $topId ($geneName) type is '$topType' not 'gene', 'pseudogene', " .
        "'rRNA', 'tRNA'";
    }
  }
  return \%linkOut;
} # collectColumns

sub parseRaFile($$) {
# Build %descriptionData and %statusData from raFile.txt
  my ($labelFile, $raFile) = @_;
  my %statusData;  # key is name, value is status
  my %descriptionData;  # key is name, value is description from gbProcess
  my %proteinId; # key is name, value is protein accession if applicable
  my @pragmaLabelRe;  # compiled regexes for labels that appear in semi-structured description text
  # Load @pragmaLabelRe (compiled regexes) for processing descriptions
  my $fh = mustOpen($labelFile);
  while (<$fh>) {
    chomp;
    my $escaped = $_;
    $escaped =~ s/\(/\\(/g;
    $escaped =~ s/\)/\\)/g;
    $escaped =~ s/\./\\./g;
    my $re = "\\s+($escaped)\\s+:: ";
    push @pragmaLabelRe, qr{$re};
  }
  close ($fh);

  $fh = mustOpen($raFile);
  my ($acc, $status, $description, $protein) = (undef, "", "", undef);
  while (<$fh>) {
    next if (/^\s*$/);
    chomp;
    my ($key, $val) = split(" ", $_, 2);
    if ($key eq "acc") {
      if (defined $acc) {
        # Store info gathered from previous record
        $statusData{$acc} = $rssToStatus{$status};
        $descriptionData{$acc} = $description;
        if ($protein) {
          $proteinId{$acc} = $protein;
        }
      }
      $acc = $val;
      ($status, $description, $protein) = ("", "", undef);
    } elsif ($key eq "ver") {
      # Add version to acc
      $acc .= ".$val";
    } elsif ($key eq "rss") {
      # rss = RefSeq Status
      $status = $val;
    } elsif ($key eq "rsu") {
      # rsu = RefSeq Summary
      # Use @pragmaLabelRe to clean up semi-structured text that was flattened by gbProcess:
      $description = $val;
      while ($description =~ /##([\w-]+)-START## /) {
        my $title = $1;
        $title =~ tr/-/ /;
        $description =~ s/##[\w-]+-START## /$title: /;
      }
      if ($description =~ s/\s+##[\w-]+-END##/./g) {
        foreach my $re (@pragmaLabelRe) {
          $description =~ s/$re/; $1: /;
        }
        $description =~ s/:;/:/g;
      }
    } elsif ($key eq 'prt') {
      $protein = $val;
    }
  }
  if (defined $acc) {
    # Store info gathered from final record
    $statusData{$acc} = $rssToStatus{$status};
    $descriptionData{$acc} = $description;
    if ($protein) {
      $proteinId{$acc} = $protein;
    }
  }
  close ($fh);
  return (\%descriptionData, \%statusData, \%proteinId);
} # parseRaFile

sub printOutput($$$$) {
  # Output tab-separated attributes
  my ($outColumns, $descriptionData, $statusData, $proteinId)  = @_;
  foreach my $id (sort keys %{$outColumns}) {
    my $idDataPtr = $outColumns->{$id};
    my $missingData = "";
    printf "%s", $id;    # first column
    # second column is the gene reviewed 'status'
    if (exists($statusData->{$id})) {
      printf "\t%s", $statusData->{$id};
    } else { printf "\tUnknown"; }
    # Make sure we found the corresponding protein for each coding transcript.
    # GFF *almost* always has it (except in rare cases when only non-coding exon(s) of
    # a coding transcript can be mapped to the assembly, so no CDS records) -- use
    # the protein ID from raFile as a backup.
    if ($id =~ m/^[NXY][MP]/ && ! $idDataPtr->{protein_id}) {
      if ($proteinId->{$id}) {
        $idDataPtr->{protein_id} = $proteinId->{$id};
      } else {
        die "Missing protein_id for coding transcript $id\n";
      }
    }
    # the rest go in order according to tabOrderOutput
    foreach my $tag ( @tabOrderOutput ) {
      if (exists($idDataPtr->{$tag})) {
        my $dataOut = $idDataPtr->{$tag};
        $dataOut =~ s/%2C/,/g;
        $dataOut =~ s/%3B/;/g;
        $dataOut =~ s/%25/%/g;
        printf "\t%s", $dataOut;
      } else { printf "\t$missingData"; }
    }
    if (exists($descriptionData->{$id})) {
      print "\t$descriptionData->{$id}";
    } else { print "\t$missingData"; }
    # last column is externalId which may or may not be applicable to this species
    my $gotExt = 0;
    foreach my $extDb (@externalDbs) {
      if (exists $idDataPtr->{$extDb} && $idDataPtr->{$extDb}) {
        print "\t" . $idDataPtr->{$extDb};
        $gotExt = 1;
        last;
      }
    }
    print "\t$missingData" if (! $gotExt);
    print "\n";
  }
} # printOutput

#################################################################################
# MAIN

my $argc = scalar(@ARGV);
if ($argc != 3) {
  &usage;
}
my ($raFile, $gffFile, $labelFile) = @ARGV;

my ($gff, $topLevelIds) = parseGff3($gffFile);
my ($outColumns) = collectColumns($gff, $topLevelIds);
my ($descriptionData, $statusData, $proteinId) = parseRaFile($labelFile, $raFile);

printOutput($outColumns, $descriptionData, $statusData, $proteinId);

