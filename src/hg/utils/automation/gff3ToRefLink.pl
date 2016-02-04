#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc != 2) {
  printf STDERR "usage: gff3ToRefLink.pl [status.tab] [ncbi.gff] > refLink.tab\n";
  printf STDERR "where status.tab is a two column file: name status\n";
  printf STDERR "and the ncbi.gff file is the gff3 file to scan\n";
  printf STDERR "output to stdout is the tab separated data for each item\n";
  exit 255;
}

# this order list of items will keep the tab output consistent so all
#  'columns' of the data in the tab output will be the same order always.
# there is one other output before these items, the gene reviewed 'status'
my @tabOrderOutput = ( 'name', 'product', 'mrnaAcc', 'protAcc', 'locusLinkId',
   'omimId', 'hgnc', 'genbank', 'pseudo', 'gbkey', 'source',
   'gene_biotype', 'gene_synonym', 'ncrna_class', 'product', 'note', 'description' );

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
            printf STDERR "warning: tag '%s' data '%s' being overwritted '%s'\n", $tag, $idDataPtr->{$tag}, $tags{$tag};
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
            printf STDERR "warning: tag '%s' data '%s' being overwritted '%s'\n", "source", $idDataPtr->{'source'}, $a[1];
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
  # stdout output is for refLink.tab table
  printf "%s", $id;
  # first column is the gene reviewed 'status'
  if (exists($statusData{$id})) {
       printf "\t%s", $statusData{$id};
  } else { printf "\tUnknown"; }
  # the rest go in order according to tabOrderOutput
  foreach my $tag ( @tabOrderOutput ) {
    if (exists($idDataPtr->{$tag})) {
       printf "\t%s", $idDataPtr->{$tag};
    } else { printf "\t"; }
  }
  printf "\n";
}
close (FH);


exit 0;

my %tagCountSurvey;  # key is tag, value is frequency count

foreach my $parent (keys %parents) {
  my $parentPtr = $parents{$parent};
  my $tagCount = 0;
  my $tagSize = 0;
  my %requiredColumns;
  printf STDERR "%s\n", $parent;
  foreach my $key (keys %$parentPtr) {
    my $tagValue = $parentPtr->{$key};
    $tagValue =~ s/%25/%/g;
    $tagValue =~ s/%2C/,/g;
    $tagValue =~ s/%3B/;/g;
    ++$tagCount;
    $tagSize += length($tagValue);
    $requiredColumns{$key} = $tagValue;
    printf STDERR "\t%s\t%s\n", $key, $tagValue if ($key ne "children");
  }
  printf "%s", $parent;
  foreach my $tag ( @tabOrderOutput ) {
    if (exists($requiredColumns{$tag})) {
       printf "\t%s", $requiredColumns{$tag};
       $tagCountSurvey{$tag}++;
    } else { printf "\t"; }
  }
  printf "\n";
  printf STDERR "# %s %d tags, length %d characters\n", $parent, $tagCount, $tagSize;
}

printf STDERR "#### tag count survey ####\n";

foreach my $tag ( @tabOrderOutput ) {
  printf STDERR "%10d\t%s\n", $tagCountSurvey{$tag}, $tag;
}

printf STDERR "##### ide to parent relation #####\n";
foreach my $id (keys %idToParent) {
   my $parentForId = $idToParent{$id};
   printf STDERR "%s\t%s\n", $parentForId, $id;
}

printf STDERR "##### parent name and children ids #####\n";

foreach my $parent (keys %parents) {
  my $parentPtr = $parents{$parent};
  if (exists($parentPtr->{'children'})) {
     my $childrenIds = $parentPtr->{'children'};
     my $parentId = $parentPtr->{'parent'};
     printf STDERR "%s", $parentId;
     foreach my $child (keys %$childrenIds) {
       printf STDERR "\t%s", $child if ($child ne $parentId);
     }
     printf STDERR "\n";
  }
}

__END__

# column 2 'source'

1022911 BestRefSeq
  12974 BestRefSeq%2CGnomon
  25744 Curated Genomic
     29 Curated Genomic%2CGnomon
1964727 Gnomon
  29865 RefSeq
   1845 tRNAscan-SE


 X 197838 name
 X 197838 locusLinkId
 X 191554 product
 X 187269 genbank
 X 187256 mrnaAcc
 X 153988 hgnc
 X 102624 protAcc
 X 100712 omimId
 X 35159 note


NC_000009.12    BestRefSeq      exon    133275162       133275214       .       -       .       ID=id755564;Parent=rna67392;Dbxref=GeneID:28,Genbank:NM_020469.2,HGNC:HGNC:79,MIM:110300;Note=The RefSeq transcript has 1 frameshift compared to this genomic sequence;exception=annotated by transcript or proteomic data;gbkey=mRNA;gene=ABO;product=ABO blood group (transferase A%2C alpha 1-3-N-acetylgalactosaminyltransferase%3B transferase B%2C alpha 1-3-galactosyltransferase);transcript_id=NM_020469.2

NC_000009.12    BestRefSeq      CDS     133262099       133262168       .       -       2       ID=cds45730;Parent=rna67392;Dbxref=GeneID:28,Genbank:NP_065202.2,HGNC:HGNC:79,MIM:110300;Name=NP_065202.2;Note=The RefSeq protein has 1 frameshift compared to this genomic sequence;exception=annotated by transcript or proteomic data;gbkey=CDS;gene=ABO;product=histo-blood group ABO system transferase;protein_id=NP_065202.2

hgsql -e 'select * from refLink where mrnaAcc="NM_020469"\G' hg38
*************************** 1. row ***************************
       name: ABO
    product: histo-blood group ABO system transferase
    mrnaAcc: NM_020469
    protAcc: NP_065202
   geneName: 65417
   prodName: 385329
locusLinkId: 28
     omimId: 110300

+-------------+------------------+------+-----+---------+-------+
| Field       | Type             | Null | Key | Default | Extra |
+-------------+------------------+------+-----+---------+-------+
| name        | varchar(255)     | NO   | MUL | NULL    |       |
| product     | varchar(255)     | NO   |     | NULL    |       |
| mrnaAcc     | varchar(255)     | NO   | PRI | NULL    |       |
| protAcc     | varchar(255)     | NO   | MUL | NULL    |       |
| geneName    | int(10) unsigned | NO   | MUL | NULL    |       |
| prodName    | int(10) unsigned | NO   | MUL | NULL    |       |
| locusLinkId | int(10) unsigned | NO   | MUL | NULL    |       |
| omimId      | int(10) unsigned | NO   | MUL | NULL    |       |
+-------------+------------------+------+-----+---------+-------+


NC_000009.12	BestRefSeq	mRNA	133255176	133275214	.	-	.	ID=rna67392;Parent=gene25179;Dbxref=GeneID:28,Genbank:NM_020469.2,HGNC:HGNC:79,MIM:110300;Name=NM_020469.2;Note=The RefSeq transcript has 1 frameshift compared to this genomic sequence;exception=annotated by transcript or proteomic data;gbkey=mRNA;gene=ABO;product=ABO blood group (transferase A%2C alpha 1-3-N-acetylgalactosaminyltransferase%3B transferase B%2C alpha 1-3-galactosyltransferase);transcript_id=NM_020469.2
NC_000009.12	BestRefSeq	exon	133275162	133275214	.	-	.	ID=id755564;Parent=rna67392;Dbxref=GeneID:28,Genbank:NM_020469.2,HGNC:HGNC:79,MIM:110300;Note=The RefSeq transcript has 1 frameshift compared to this genomic sequence;exception=annotated by transcript or proteomic data;gbkey=mRNA;gene=ABO;product=ABO blood group (transferase A%2C alpha 1-3-N-acetylgalactosaminyltransferase%3B transferase B%2C alpha 1-3-galactosyltransferase);transcript_id=NM_020469.2
NC_000009.12	BestRefSeq	exon	133262099	133262168	.	-	.	ID=id755565;Parent=rna67392;Dbxref=GeneID:28,Genbank:NM_020469.2,HGNC:HGNC:79,MIM:110300;Note=The RefSeq transcript has 1 frameshift compared to this genomic sequence;exception=annotated by transcript or proteomic data;gbkey=mRNA;gene=ABO;product=ABO blood group (transferase A%2C alpha 1-3-N-acetylgalactosaminyltransferase%3B transferase B%2C alpha 1-3-galactosyltransferase);transcript_id=NM_020469.2
NC_000009.12	BestRefSeq	exon	133261318	133261374	.	-	.	ID=id755566;Parent=rna67392;Dbxref=GeneID:28,Genbank:NM_020469.2,HGNC:HGNC:79,MIM:110300;Note=The RefSeq transcript has 1 frameshift compared to this genomic sequence;exception=annotated by transcript or proteomic data;gbkey=mRNA;gene=ABO;product=ABO blood group (transferase A%2C alpha 1-3-N-acetylgalactosaminyltransferase%3B transferase B%2C alpha 1-3-galactosyltransferase);transcript_id=NM_020469.2
NC_000009.12	BestRefSeq	exon	133259819	133259866	.	-	.	ID=id755567;Parent=rna67392;Dbxref=GeneID:28,Genbank:NM_020469.2,HGNC:HGNC:79,MIM:110300;Note=The RefSeq transcript has 1 frameshift compared to this genomic sequence;exception=annotated by transcript or proteomic data;gbkey=mRNA;gene=ABO;product=ABO blood group (transferase A%2C alpha 1-3-N-acetylgalactosaminyltransferase%3B transferase B%2C alpha 1-3-galactosyltransferase);transcript_id=NM_020469.2
NC_000009.12	BestRefSeq	exon	133258097	133258132	.	-	.	ID=id755568;Parent=rna67392;Dbxref=GeneID:28,Genbank:NM_020469.2,HGNC:HGNC:79,MIM:110300;Note=The RefSeq transcript has 1 frameshift compared to this genomic sequence;exception=annotated by transcript or proteomic data;gbkey=mRNA;gene=ABO;product=ABO blood group (transferase A%2C alpha 1-3-N-acetylgalactosaminyltransferase%3B transferase B%2C alpha 1-3-galactosyltransferase);transcript_id=NM_020469.2
NC_000009.12	BestRefSeq	exon	133257409	133257542	.	-	.	ID=id755569;Parent=rna67392;Dbxref=GeneID:28,Genbank:NM_020469.2,HGNC:HGNC:79,MIM:110300;Note=The RefSeq transcript has 1 frameshift compared to this genomic sequence;exception=annotated by transcript or proteomic data;gbkey=mRNA;gene=ABO;product=ABO blood group (transferase A%2C alpha 1-3-N-acetylgalactosaminyltransferase%3B transferase B%2C alpha 1-3-galactosyltransferase);transcript_id=NM_020469.2
NC_000009.12	BestRefSeq	exon	133255176	133256356	.	-	.	ID=id755570;Parent=rna67392;Dbxref=GeneID:28,Genbank:NM_020469.2,HGNC:HGNC:79,MIM:110300;Note=The RefSeq transcript has 1 frameshift compared to this genomic sequence;exception=annotated by transcript or proteomic data;gbkey=mRNA;gene=ABO;product=ABO blood group (transferase A%2C alpha 1-3-N-acetylgalactosaminyltransferase%3B transferase B%2C alpha 1-3-galactosyltransferase);transcript_id=NM_020469.2
NC_000009.12	BestRefSeq	CDS	133275162	133275189	.	-	0	ID=cds45730;Parent=rna67392;Dbxref=GeneID:28,Genbank:NP_065202.2,HGNC:HGNC:79,MIM:110300;Name=NP_065202.2;Note=The RefSeq protein has 1 frameshift compared to this genomic sequence;exception=annotated by transcript or proteomic data;gbkey=CDS;gene=ABO;product=histo-blood group ABO system transferase;protein_id=NP_065202.2
NC_000009.12	BestRefSeq	CDS	133262099	133262168	.	-	2	ID=cds45730;Parent=rna67392;Dbxref=GeneID:28,Genbank:NP_065202.2,HGNC:HGNC:79,MIM:110300;Name=NP_065202.2;Note=The RefSeq protein has 1 frameshift compared to this genomic sequence;exception=annotated by transcript or proteomic data;gbkey=CDS;gene=ABO;product=histo-blood group ABO system transferase;protein_id=NP_065202.2
NC_000009.12	BestRefSeq	CDS	133261318	133261374	.	-	1	ID=cds45730;Parent=rna67392;Dbxref=GeneID:28,Genbank:NP_065202.2,HGNC:HGNC:79,MIM:110300;Name=NP_065202.2;Note=The RefSeq protein has 1 frameshift compared to this genomic sequence;exception=annotated by transcript or proteomic data;gbkey=CDS;gene=ABO;product=histo-blood group ABO system transferase;protein_id=NP_065202.2
NC_000009.12	BestRefSeq	CDS	133259819	133259866	.	-	1	ID=cds45730;Parent=rna67392;Dbxref=GeneID:28,Genbank:NP_065202.2,HGNC:HGNC:79,MIM:110300;Name=NP_065202.2;Note=The RefSeq protein has 1 frameshift compared to this genomic sequence;exception=annotated by transcript or proteomic data;gbkey=CDS;gene=ABO;product=histo-blood group ABO system transferase;protein_id=NP_065202.2
NC_000009.12	BestRefSeq	CDS	133258097	133258132	.	-	1	ID=cds45730;Parent=rna67392;Dbxref=GeneID:28,Genbank:NP_065202.2,HGNC:HGNC:79,MIM:110300;Name=NP_065202.2;Note=The RefSeq protein has 1 frameshift compared to this genomic sequence;exception=annotated by transcript or proteomic data;gbkey=CDS;gene=ABO;product=histo-blood group ABO system transferase;protein_id=NP_065202.2
NC_000009.12	BestRefSeq	CDS	133257409	133257542	.	-	1	ID=cds45730;Parent=rna67392;Dbxref=GeneID:28,Genbank:NP_065202.2,HGNC:HGNC:79,MIM:110300;Name=NP_065202.2;Note=The RefSeq protein has 1 frameshift compared to this genomic sequence;exception=annotated by transcript or proteomic data;gbkey=CDS;gene=ABO;product=histo-blood group ABO system transferase;protein_id=NP_065202.2
NC_000009.12	BestRefSeq	CDS	133255666	133256356	.	-	2	ID=cds45730;Parent=rna67392;Dbxref=GeneID:28,Genbank:NP_065202.2,HGNC:HGNC:79,MIM:110300;Name=NP_065202.2;Note=The RefSeq protein has 1 frameshift compared to this genomic sequence;exception=annotated by transcript or proteomic data;gbkey=CDS;gene=ABO;product=histo-blood group ABO system transferase;protein_id=NP_065202.2
