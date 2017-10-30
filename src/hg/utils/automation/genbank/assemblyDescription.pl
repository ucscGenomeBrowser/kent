#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 1) {
  printf STDERR "usage: assemblyDescription.pl <buildDirectory>\n";
  printf STDERR "where <buildDirectory> is where the assembly hub is under construction.\n";
  exit 255;
}

my $wrkDir = shift;

if ( ! -d $wrkDir ) {
  printf STDERR "ERROR: can not find work directory:\n\t'%s'\n", $wrkDir;
  exit 255;
}

chdir $wrkDir;
# print STDERR `ls`;
my $accAsm = basename($wrkDir);
exit 0 if ( ! -s "bbi/${accAsm}.assembly.ncbi.bb" );
my $groupName = $wrkDir;
my $asmType = "genbank";
$asmType = "refseq" if ( $groupName =~ m#/refseq/#);
$groupName =~ s#.*/$asmType/##;
$groupName =~ s#/.*##;
my $selfUrl = "${groupName}/${accAsm}";

# printf STDERR "%s\n", $accAsm;
my $htmlFile="${accAsm}.assembly.html";
open (FH, ">$htmlFile") or die "can not write to $htmlFile";

my $em = "<em>";
my $noEm = "</em>";
my $assemblyDate = `grep -v "^#" $accAsm.names.tab | cut -f9`;
chomp $assemblyDate;
my $ncbiAssemblyId = `grep -v "^#" $accAsm.names.tab | cut -f10`;
chomp $ncbiAssemblyId;
my $agpFiles = `ls ${accAsm}.*.agp.ncbi.gz | xargs echo`;
chomp $agpFiles;
my $organism = `grep -v "^#" $accAsm.names.tab | cut -f5`;
chomp $organism;


print FH <<_EOF_
<h2>Description</h2>
<p>
This track shows the sequences used in the $assemblyDate $em${organism}$noEm/$accAsm genome assembly.
</p>
<p>
Genome assembly procedures are covered in the NCBI
<a href="https://www.ncbi.nlm.nih.gov/assembly/basics/"
target=_blank>assembly documentation</a>.<BR>
NCBI also provides
<a href="https://www.ncbi.nlm.nih.gov/assembly/$ncbiAssemblyId"
target="_blank">specific information about this assembly</a>.
</p>
<p>
The definition of the sequences in this assembly is from the AGP file(s):<br>
$agpFiles<br>
in this <a href="/gbdb/hubs/$asmType/$selfUrl/" target=_blank>build directory</a><br>
The NCBI document
<a href="https://www.ncbi.nlm.nih.gov/assembly/agp/AGP_Specification/"
target=_blank>AGP Specification</a> describes the format of the AGP file.
</p>
<p>
In dense mode, this track depicts the contigs that make up the 
currently viewed scaffold. 
Contig boundaries are distinguished by the use of alternating gold and brown 
coloration. Where gaps
exist between contigs, spaces are shown between the gold and brown
blocks.  The relative order and orientation of the contigs
within a scaffold is always known; therefore, a line is drawn in the graphical
display to bridge the blocks.</p>
<p>
Return to UCSC naming <a href="/gbdb/hubs/$asmType/$groupName/${groupName}.html">$groupName</a> assembly hub index.<br>
Return to NCBI naming  <a href="/gbdb/hubs/$asmType/$groupName/${groupName}.ncbi.html">$groupName</a> assembly hub index.
</p>
_EOF_
    ;

close (FH);
