#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc != 3) {
  printf STDERR "usage: gff3ToRefLink.pl [raFile.txt] [ncbi.gff.gz] [pragmaLabels.txt] > refLink.tab\n";
  printf STDERR "- raFile.txt is the output of gbProcess \${asmId}_rna.gbff.gz\n";
  printf STDERR "- ncbi.gff.gz is \${asmId}_genomic.gff.gz\n";
  printf STDERR "- pragmaLabels.txt has words that appear before '::' within ##*-START/##*-END\n";
  printf STDERR "  text in \${asmId}_rna.gbff.gz, one label per line.\n";
  printf STDERR "output to stdout is the tab separated refLink data for each item\n";
  exit 255;
}

my ($raFile, $gffFile, $labelFile) = @ARGV;

# this order list of items will keep the tab output consistent so all
#  'columns' of the data in the tab output will be the same order always.
# there is one other output before these items, the gene reviewed 'status'
# and one column after these items, the 'description' string from gbProcess
my @tabOrderOutput = ( 'name', 'product', 'mrnaAcc', 'protAcc', 'locusLinkId',
   'omimId', 'hgnc', 'genbank', 'pseudo', 'gbkey', 'source',
   'gene_biotype', 'gene_synonym', 'ncrna_class', 'note' );

# raFile.txt has 3-letter abbreviated status (or no status given for model-only X[MR]_*)
my %rssToStatus = ( inf => 'Inferred',
                    ''  => 'Model',
                    pre => 'Predicted',
                    pro => 'Provisional',
                    rev => 'Reviewed',
                    val => 'Validated' );

# list of tag names that have dynamic content for each line, no need to
# record this in the global set of tags
my %dynamicTags = ( "end_range" => 1, "gap" => 1, "idty" => 1,
    "score" => 1, "start_range" => 1, "target" => 1 );

my %parents;   # key is a parent ID, value is a pointer to
               # a hash of tags and values
my %idToParent;  # key is an id, value is its parent
my %idData;    # key is an ID, value is a pointer to a hash for
               # for the tags and values on this id
my %statusData;  # key is name, value is status
my @pragmaLabelRe;  # compiled regexes for labels that appear in semi-structured description text
my %descriptionData;  # key is name, value is description from gbProcess


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

# Build %descriptionData and %statusData from raFile.txt
$fh = mustOpen($raFile);
my ($acc, $status, $description) = (undef, "", "");
while (<$fh>) {
  next if (/^\s*$/);
  chomp;
  my ($key, $val) = split(" ", $_, 2);
  if ($key eq "acc") {
    if (defined $acc) {
      # Store info gathered from previous record
      $statusData{$acc} = $rssToStatus{$status};
      $descriptionData{$acc} = $description;
    }
    $acc = $val;
    ($status, $description) = ("", "");
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
  }
}
if (defined $acc) {
  # Store info gathered from final record
  $statusData{$acc} = $rssToStatus{$status};
  $descriptionData{$acc} = $description;
}
close ($fh);

# Process GFF3 attributes
$fh = mustOpen($gffFile);
while (my $line = <$fh>) {
  chomp $line;
  next if ($line =~ m/^#/);
  my @a = split('\t', $line);
  next if($a[2] eq "region");
  if (1 == 1 || $a[8] =~ m/dbxref/i) {
    my @b = split(';', $a[8]);
    my %tags;
    foreach my $tagValue (@b) {
       my ($tag, $value) = split('=', $tagValue);
       $tags{lc($tag)} = $value;
    }
    if (exists($tags{'id'})) {
      my $id = $tags{'id'};
      if (! exists($idData{$id})) {
         my %tagData;
         $idData{$id} = \%tagData;
      }
    } else {
      die "# ERROR: in item without an id ? '$line'\n$a[8]\n";
    }
    my $idDataPtr = $idData{$tags{'id'}};
    # simply dump all tags into this id pointer:
    foreach my $tag (sort keys %tags) {
       next if (exists($dynamicTags{$tag})); # do not record dynamic tags
       if (exists($idDataPtr->{$tag})) {
          if ($idDataPtr->{$tag} ne $tags{$tag}) {
            printf STDERR "warning: tag '%s' data '%s' being overwritten '%s'\n", $tag, $idDataPtr->{$tag}, $tags{$tag};
          }
       } else {
          $idDataPtr->{$tag} = $tags{$tag};
       }
    }
    if (exists($tags{'parent'})) {
       my $parent = $tags{'parent'};
       if (! exists($parents{$parent}) ) {
          my %newTag;
          $parents{$parent} = \%newTag;
          $parents{$parent}->{'parent'} = $parent;
       }
       my $parentPtr = $parents{$parent};
       # if this parent has already been seen to have a parent, than
       # that 'grand' parent is the parent of this child.
       if (exists($idToParent{$tags{'parent'}})) {
         $idToParent{$tags{'id'}} = $idToParent{$tags{'parent'}};
       } else {
         $idToParent{$tags{'id'}} = $tags{'parent'};   # new parent to id
       }
       $parentPtr->{'source'} = $a[1];  # capture column 2 'source'
       if (exists($idDataPtr->{'source'})) {
         if ($idDataPtr->{'source'} ne $a[1]) {
            printf STDERR "warning: tag '%s' data '%s' being overwritten '%s'\n", "source", $idDataPtr->{'source'}, $a[1];
         }
       } else {
         $idDataPtr->{'source'} = $a[1];
       }
       if (exists($tags{'dbxref'})) {
          my @c = split(',', $tags{'dbxref'});
          foreach my $xref (@c) {
             if ($xref =~ m/^geneid:/i) {
               my ($tag, $value) = split(':', $xref);
               $idDataPtr->{'locusLinkId'} = $value;
             }
             if ($xref =~ m/^mim:/i) {
               my ($tag, $value) = split(':', $xref);
               $idDataPtr->{'omimId'} = $value;
             }
             if ($xref =~ m/^genbank:/i) {
               my ($tag, $value) = split(':', $xref);
               $idDataPtr->{'genbank'} = $value;
             }
             if ($xref =~ m/^hgnc:/i) {
               my ($tag, $tag2, $value) = split(':', $xref);
               $idDataPtr->{'hgnc'} = $value;
             }
             if ($xref =~ m/^mgi:/i) {
               my ($tag, $tag2, $value) = split(':', $xref);
               $idDataPtr->{'mgi'} = uc($tag2).":$value";
             }
          }
       }
       # a couple of renames for tag names
       if (exists($tags{'transcript_id'})) {
          $idDataPtr->{'mrnaAcc'} = $tags{'transcript_id'};
       }
       if (exists($tags{'protein_id'})) {
          $idDataPtr->{'protAcc'} = $tags{'protein_id'};
       }
       if (exists($tags{'gene'})) {
          $idDataPtr->{'name'} = $tags{'gene'};
       }
    }
  }
}
close ($fh);

my %addAlias;  # key is id from idData, value is name to alias

# verify all tags in children are also in their parents, carry up when possible
foreach my $id (keys %idData) {
  my $idDataPtr = $idData{$id};
  if (exists($idDataPtr->{'parent'})) {
     my $parent = $idDataPtr->{'parent'};
     if (exists($idData{$parent})) {
       my $parentPtr = $idData{$parent};
       foreach my $tag (keys %$idDataPtr) {
         next if (($tag eq "id") || ($tag eq "parent"));
         if (exists($parentPtr->{$tag})) {
           my $parentData = $parentPtr->{$tag};
           if ($parentData ne $idDataPtr->{$tag}) {
              printf STDERR "# note different data for tag %s from parent %s from child %s\n", $tag, $parent, $id;
           }  # else data is identical between child and parent
         } else {
           printf STDERR "# note new tag %s for parent %s from child %s\n", $tag, $parent, $id;
           $parentPtr->{$tag} = $idDataPtr->{$tag};
         }
       }
     } else {
       printf STDERR "# warning parent %s is a new id ?\n", $parent;
     }
  } else {
    printf STDERR "# note no parent for id %s\n", $id;
  }
  if (exists ($idDataPtr->{'genbank'})) {     # if a 'genbank' name exists
     my $aliasName = $idDataPtr->{'genbank'};
     if (! exists($addAlias{$aliasName}) ) { # and there isn't an alias yet
       if ($id =~ m/^gene|^rna|^cds/) {
          printf STDERR "# requesting alias $aliasName for $id\n";
          $addAlias{$aliasName} = $id;      # add alias if this is a gene or rna
       }
     }
  }
  if (exists ($idDataPtr->{'mrnaAcc'})) {     # if a 'mrnaAcc' name exists
     my $aliasName = $idDataPtr->{'mrnaAcc'};
     if (! exists($addAlias{$aliasName}) ) { # and there isn't an alias yet
       if ($id =~ m/^gene|^rna|^cds/) {
          printf STDERR "# requesting alias $aliasName for $id\n";
          $addAlias{$aliasName} = $id;      # add alias if this is a gene or rna
       }
     }
  }
  if (exists ($idDataPtr->{'name'})) {     # if a 'mrnaAcc' name exists
     my $aliasName = $idDataPtr->{'name'};
     if (! exists($addAlias{$aliasName}) ) { # and there isn't an alias yet
       if ($id =~ m/^gene|^rna|^cds/) {
          printf STDERR "# requesting alias $aliasName for $id\n";
          $addAlias{$aliasName} = $id;      # add alias if this is a gene or rna
       }
     }
  }
}

# catch up with the alias requests
foreach my $aliasName (keys %addAlias) {
  my $id = $addAlias{$aliasName};
  if (! exists($idData{$aliasName})) {  # if not done, create it
     my $idDataPtr = $idData{$id};
     my %tagData;
     foreach my $tag (keys %$idDataPtr) {  # copy data tags to alias
        $tagData{$tag} = $idDataPtr->{$tag};
     }
     printf STDERR "# adding alias $aliasName for $id\n";
     $idData{$aliasName} = \%tagData;       # becomes a new item of data
  }
}

# Output tab-separated attributes
foreach my $id (keys %idData) {
  my $idDataPtr = $idData{$id};
  my $missingData = "n/a";
  # stdout output is for refLink.tab file - precursor of ncbiRefSeqLink.tab
  printf "%s", $id;    # first column
  # second column is the gene reviewed 'status'
  if (exists($statusData{$id})) {
       printf "\t%s", $statusData{$id};
  } else { printf "\tUnknown"; }
  # the rest go in order according to tabOrderOutput
  foreach my $tag ( @tabOrderOutput ) {
    if (exists($idDataPtr->{$tag})) {
       my $dataOut = $idDataPtr->{$tag};
       $dataOut =~ s/%2C/,/g;
       $dataOut =~ s/%3B/;/g;
       $dataOut =~ s/%25/%/g;
       printf "\t%s", $dataOut;
    } elsif ($tag eq 'hgnc' && exists $idDataPtr->{mgi}) {
       printf "\t" . $idDataPtr->{mgi};
    } else { printf "\t$missingData"; }
  }
  if (exists($descriptionData{$id})) {
    print "\t$descriptionData{$id}\n";
  } else { print "\t$missingData\n"; }
}
