#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1) {
  printf STDERR "usage: ./prepOne.pl <dbId>\n";
  printf STDERR "where dbId is one of the names from the dbId.list here\n";
  exit 255;
}

my $targetDb = "galGal6";

my $dbId = shift;
my $DbId = ucfirst($dbId);
my $dateStamp = `date "+%Y-%m-%d"`;
chomp $dateStamp;

my $wrkDir = sprintf("/hive/data/genomes/%s/bed/lastz%s.%s", $targetDb, $DbId, $dateStamp);
my $baseWrkDir = `basename "$wrkDir"`;
chomp $baseWrkDir;

print `mkdir -p $wrkDir`;

if ( -s "${wrkDir}/fb.$targetDb.chainRBest.$DbId.txt" ) {
  printf STDERR "# maybe already done: $dbId\n";
  exit 0;
}
if ( -s "${wrkDir}/fb.$targetDb.chainSyn${DbId}Link.txt" ) {
  printf STDERR "# partially done: $dbId\n";
  exit 255;
}
if ( -s "${wrkDir}/fb.$targetDb.chain${DbId}Link.txt" ) {
  printf STDERR "# partially done: $dbId\n";
  exit 255;
}

if ( -d "${wrkDir}/run.blastz" ) {
  printf STDERR "# may be running now: $dbId\n";
  exit 255;
}

my $defFile = "${wrkDir}/DEF";

my @testFiles = ( "/hive/data/genomes/$targetDb/$targetDb.2bit", "/hive/data/genomes/$targetDb/chrom.sizes", "/hive/data/genomes/$dbId/$dbId.2bit", "/hive/data/genomes/$dbId/chrom.sizes" );
my $broken = 0;
foreach my $testFile (@testFiles) {
  if ( ! -s "${testFile}" ) {
     ++$broken;
     printf STDERR "ERROR: can not find file: '%s'\n", $testFile;
  }
}

die "can not find all required files" if ($broken);

printf STDERR "# preparing: %s\n", $wrkDir;
my $orgName = `hgsql -N -e 'select organism from dbDb where name="$dbId";' hgcentraltest`;
chomp $orgName;
my $sciName = `hgsql -N -e 'select scientificName from dbDb where name="$dbId";' hgcentraltest`;
chomp $sciName;

if ( ! -e "${defFile}" ) {
  open(FH, ">${defFile}") or die "can not write to ${defFile}";
  printf FH "# $targetDb vs $dbId
BLASTZ=/cluster/bin/penn/lastz-distrib-1.04.00/bin/lastz
BLASTZ_H=2000
BLASTZ_Y=3400
BLASTZ_L=4000
BLASTZ_K=2200
BLASTZ_M=254
BLASTZ_Q=/cluster/data/blastz/HoxD55.q

# TARGET: $targetDb - chicken - Gallus gallus
SEQ1_DIR=/hive/data/genomes/galGal6/galGal6.2bit
SEQ1_LEN=/hive/data/genomes/galGal6/chrom.sizes
SEQ1_CHUNK=20000000
SEQ1_LAP=10000
SEQ1_LIMIT=40

# QUERY: $dbId - $orgName - $sciName
SEQ2_DIR=/hive/data/genomes/$dbId/$dbId.2bit
SEQ2_LEN=/hive/data/genomes/$dbId/chrom.sizes
SEQ2_CHUNK=1000000
SEQ2_LIMIT=2000
SEQ2_LAP=0

BASE=${wrkDir}
TMPDIR=/dev/shm
";
  close (FH);
} else {
  printf STDERR "# already exists: ${wrkDir}/DEF\n";
  exit 255;
}

if ( -e "${wrkDir}/runAlignment.sh" ) {
  printf STDERR "# already exists: ${wrkDir}/runAlignment.sh\n";
} else {
  open (FH, ">${wrkDir}/runAlignment.sh") or die "can not write to ${wrkDir}/runAlignment.sh";
  printf FH "#!/bin/bash
cd %s
if [ -s \"fb.%s.chainSyn%sLink.txt\" ]; then
  exit 0
fi
if [ -s \"fb.%s.chain%sLink.txt\" ]; then
  printf \"not completely done %s\\n\" 1>&2
  exit 255
fi
if [ -d \"run.blastz\" ]; then
  printf \"may be running now %s\\n\" 1>&2
  exit 255
fi

", $wrkDir, $targetDb, $DbId, $targetDb, $DbId, $baseWrkDir, $baseWrkDir ;

  print FH 'doBlastzChainNet.pl `pwd`/DEF -verbose=2 \\
    -workhorse=hgwdev -bigClusterHub=ku \\
	-dbHost=hgwdev -smallClusterHub=ku \\
	    -fileServer=hgwdev -syntenicNet' ;
  printf FH "\n";

  printf FH "\n#####################  reciprocal best   ########\n";
  printf FH "\nif [ ! -s \"fb.%s.chainSyn%sLink.txt\" ]; then
  printf \"not completely done %s\\n\" 1>&2
  exit 255
fi
", $targetDb, $DbId, $baseWrkDir;

printf FH "echo \"# working '%s' \"`date '+%%s %%F %%T'` > rbest.log\n", $baseWrkDir;

print FH 'export tDb=`grep "SEQ1_DIR=" DEF | sed -e "s#.*/##; s#.2bit##;"`
export qDb=`grep "SEQ2_DIR=" DEF | sed -e "s#.*/##; s#.2bit##;"`
export target2Bit=`grep "SEQ1_DIR=" DEF | sed -e "s/.*=//;"`
export targetSizes=`grep "SEQ1_LEN=" DEF | sed -e "s/.*=//;"`
export query2Bit=`grep "SEQ2_DIR=" DEF | sed -e "s/.*=//;"`
export querySizes=`grep "SEQ2_LEN=" DEF | sed -e "s/.*=//;"`

time (doRecipBest.pl -buildDir=`pwd` -load \
  -workhorse=hgwdev -dbHost=hgwdev \
   -target2Bit=${target2Bit} -query2Bit=${query2Bit} \
    -targetSizes=${targetSizes} -querySizes=${querySizes} \
      ${tDb} ${qDb}) >> rbest.log 2>&1
';

printf FH "echo \"# done '%s' \"`date '+%%s %%F %%T'` >> rbest.log\n", $baseWrkDir;

  close (FH);
  print `chmod +x ${wrkDir}/runAlignment.sh`;
}
