#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc != 3) {
  printf STDERR "usage: gff3ToRefLink.pl [status.tab] [descriptions.tab] [ncbi.gff] > refLink.tab\n";
  printf STDERR "where status.tab is a two column file: name status\n";
  printf STDERR "where descriptions.tab is a two column file: name description\n";
  printf STDERR "and the ncbi.gff file is the gff3 file to scan\n";
  printf STDERR "output to stdout is the tab separated data for each item\n";
  exit 255;
}

# this order list of items will keep the tab output consistent so all
#  'columns' of the data in the tab output will be the same order always.
# there is one other output before these items, the gene reviewed 'status'
# and one column after these items, the 'description' string from gbProcess
my @tabOrderOutput = ( 'name', 'product', 'mrnaAcc', 'protAcc', 'locusLinkId',
   'omimId', 'hgnc', 'genbank', 'pseudo', 'gbkey', 'source',
   'gene_biotype', 'gene_synonym', 'ncrna_class', 'note' );

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
my %descriptionData;  # key is name, value is description from gbProcess

my $statusFile = shift;
if ($statusFile =~ m/.gz$/) {
  open (FH, "zcat $statusFile|") or die "can not read $statusFile";
} elsif ($statusFile =~ m/^stdin$/) {
  open (FH, "</dev/stdin") or die "can not read $statusFile";
} else {
  open (FH, "<$statusFile") or die "can not read $statusFile";
}
while (my $line = <FH>) {
  chomp $line;
  my ($name, $status) = split('\s+', $line);
  $statusData{$name} = ucfirst(lc($status));
}
close (FH);

my $descriptionFile = shift;
if ($descriptionFile =~ m/.gz$/) {
  open (FH, "zcat $descriptionFile|") or die "can not read $descriptionFile";
} elsif ($descriptionFile =~ m/^stdin$/) {
  open (FH, "</dev/stdin") or die "can not read $descriptionFile";
} else {
  open (FH, "<$descriptionFile") or die "can not read $descriptionFile";
}
while (my $line = <FH>) {
  chomp $line;
  my ($name, $description) = split('\t', $line, 2);
  $descriptionData{$name} = $description if (defined($name));
}
close (FH);

my $file = shift;
if ($file =~ m/.gz$/) {
  open (FH, "zcat $file|") or die "can not read $file";
} elsif ($file =~ m/^stdin$/) {
  open (FH, "</dev/stdin") or die "can not read $file";
} else {
  open (FH, "<$file") or die "can not read $file";
}
while (my $line = <FH>) {
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
close (FH);

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

open (FH, ">idTags.tab") or die "can not write to idTabs.tab";
foreach my $id (keys %idData) {
  my $idDataPtr = $idData{$id};
  printf FH "%s\n", $id;
  foreach my $dataTag (sort keys %$idDataPtr) {
    printf FH "\t%s\t%s\n", $dataTag, $idDataPtr->{$dataTag}
  }
  my $idNoVersion = $id;
  $idNoVersion =~ s/.[0-9]+$//;
  my $description = "n/a";    # missing data says 'n/a'
  if (exists($descriptionData{$idNoVersion})) {
     $description = $descriptionData{$idNoVersion};
  }
  # stdout output is for refLink.tab table
  printf "%s", $id;    # first column
  # first column is the gene reviewed 'status'
  if (exists($statusData{$id})) {
       printf "\t%s", $statusData{$id};  # second column
  } else { printf "\tUnknown"; }
  # the rest go in order according to tabOrderOutput
  foreach my $tag ( @tabOrderOutput ) {
    if (exists($idDataPtr->{$tag})) {
       my $dataOut = $idDataPtr->{$tag};
       $dataOut =~ s/%2C/,/g;
       $dataOut =~ s/%3B/;/g;
       $dataOut =~ s/%25/%/g;
       printf "\t%s", $dataOut;
    } else { printf "\tn/a"; }    # missing data says 'n/a'
  }
  printf "\t%s\n", $description;  # last column
}
close (FH);
