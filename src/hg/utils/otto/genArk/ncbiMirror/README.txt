##############################################################################
##############################################################################

The processes described here create the files that are needed by
the 'otto' process which rebuilds the hgcentral.genark table
and the hgcentral.assemblyList table.

##############################################################################
##############################################################################
There is a nested hierarchy of scripts descibed here.  They are
running from the single cron job script ncbiRsync.sh.
The hierarchy structure of ncbiRsync.sh:

   1. ncbiRsync.sh
      2. fetch.sh
         3. updateLists.sh genbank|refseq
            4. taxonomyNames.pl
            4. commonNames.pl
            4. allVerts.sh - probably outdated
            4. primates.pl - probably outdated
         3. catLists.sh genbank|refseq
         3. genbank.sh or refseq.sh
            4. genbankToDo.sh or refseqToDo.sh
         3. cladesToday.sh
         3. cronUpdate.sh (once a week, only during genbank)
            4. genBank/rerunKu.sh - prep for ku cluster job
               5. kuBatch.sh - ku cluster job run from rerunKu.sh via ssh ku
                  6. runOne - individual cluster job
                     7. commonNames.py - in source tree makeDb/doc/asmHubs/
            4. refSeq/rerunHgwdev.sh - preps for and runs hgwdev cluster job
               5. runOne - individual cluster job
                  6. commonNames.py - in source tree makeDb/doc/asmHubs/


##############################################################################
######## ncbiRsync.sh ########

The ncbiRsync.sh cron job is run twice a week,  (Hiram's cron tab)

once for the GenBank/GCA sequences, and then the RefSeq/GCF sequences:

### 05 00 * * 3 /hive/data/outside/ncbi/genomes/cronUpdates/ncbiRsync.sh GCA
### 05 00 * * 6 /hive/data/outside/ncbi/genomes/cronUpdates/ncbiRsync.sh GCF

RefSeq/GCF run on Saturday, usually finishes by Monday
GenBank/GCA runs on Wednesday, usually finishes by Friday

Do not want these things to overlap since they are running an
rsync to the same service at NCBI.

It runs an rsync to maintain a mirror from:
  rsync://ftp.ncbi.nlm.nih.gov/genomes/all/[GCA|GCF]/

into local space:

  /hive/data/outside/ncbi/genomes/GCA/
  /hive/data/outside/ncbi/genomes/GCF/

The process keeps log files in:
  /hive/data/outside/ncbi/genomes/cronUpdates/log/YYYY/MM/...

##############################################################################
######## fetch.sh ########

That ncbiRsync.sh continues processing with the 'fetch.sh'
script in:

  /hive/data/outside/ncbi/genomes/reports/fetch.sh

This script copies the meta data text files from:

rsync://ftp.ncbi.nlm.nih.gov/genomes/ASSEMBLY_REPORTS/assembly_summary_genbank*
rsync://ftp.ncbi.nlm.nih.gov/genomes/ASSEMBLY_REPORTS/assembly_summary_refseq*

and a number of other files from that ASSEMBLY_REPORTS directory
into local directory:

  /hive/data/outside/ncbi/genomes/reports/

These files are very useful for scanning everything that is
available from NCBI.

That 'fetch.sh' script continues processing with the scripts
in /hive/data/outside/ncbi/genomes/reports/genbank/
and /hive/data/outside/ncbi/genomes/reports/refseq
specifically, for GenBank/GCA:
    ./updateLists.sh genbank
    ./catLists.sh genbank
    /hive/data/outside/ncbi/genomes/reports/newAsm/genbank.sh

and for RefSeq/GCF:
    ./updateLists.sh refseq
    ./catLists.sh refseq
    /hive/data/outside/ncbi/genomes/reports/newAsm/refseq.sh

Those updateLists.sh and catLists.sh are the same file
in each of these refSeq/genBank directories.

And it runs the script:

  /hive/data/outside/ncbi/genomes/reports/newAsm/cladesToday.sh
and once a week (after the GenBank/GCA mirror rsync0:
  /hive/data/outside/ncbi/genomes/reports/allCommonNames/cronUpdate.sh

##############################################################################
######## updateLists.sh ########

This script is going maintain files for taxonomy and
common names listings.  It is working in the
directory:
   /hive/data/outside/ncbi/genomes/reports/{genbank,refseq}/
depending upon which one it is running.

It keeps log files in:
  /hive/data/outside/ncbi/genomes/reports/genbank/history/YYYY/MM
  /hive/data/outside/ncbi/genomes/reports/refseq/history/YYYY/MM
and history files for these listings it is making.

It performs this work with the scripts:
  ./taxonomyNames.pl (two scripts, nearly identical)
  ./commonNames.pl (two scripts, nearly identical)
  ./allVerts.sh - probably outdated
  ./primates.pl - probably outdated

##############################################################################
######## {refseq,genbank}.taxonomyNames.pl ########

These two scripts are run by updateLists.sh

There are two copies of this script, they are identical except
for their usage message:

  diff refseq.taxonomyNames.pl genbank.taxonomyNames.pl
9c9
<   printf STDERR "usage: taxonomyNames.pl ../../refseq/assembly_summary_refseq.txt [skipAsmIds]\n";
---
>   printf STDERR "usage: taxonomyNames.pl ../../genbank/assembly_summary_genbank.txt [skipAsmIds]\n";

  This script works through all the new assembly identifiers since
  the last update and uses calls to https://eutils.ncbi.nlm.nih.gov/
  to obtain the taxonomy strings from NCBI taxonomy database.
  It reads in the existing file from previous updates:
    allAssemblies.taxonomy.tsv
  and then anything new to add is added to the end of that file,
  rewriting the thing.  Before the update, the file is saved by
  the updateListings.sh script in the history log file hierarchy.

##############################################################################
######## {refseq,genbank}.commonNames.pl ########

These two scripts are run by updateLists.sh

Similar to taxonomyNames.pl, updates the existing file:
  allAssemblies.commonNames.tsv
using calls to https://eutils.ncbi.nlm.nih.gov/

##############################################################################
######## catLists.sh ########

Run by updateLists.sh in both directories:
  /hive/data/outside/ncbi/genomes/reports/{genbank,refseq}/catLists.sh
One single copy of this file, hard linked together to appear
in these two directories.

This script uses the allAssemblies.taxonomy.tsv listings maintained
by taxonomyNames.pl to divide the taxonomies into GenArk
clade listings.  These listings accumulate in the log directory
hierarchy:

  /hive/data/outside/ncbi/genomes/reports/genbank/history/YYYY/MM
  /hive/data/outside/ncbi/genomes/reports/refseq/history/YYYY/MM

##############################################################################
######## genbank.sh and refseq.sh ########

Scripts run by updateLists.sh in the directory:

  /hive/data/outside/ncbi/genomes/reports/newAsm/

These scripts split up the allAssemblies.taxonomy.tsv listing
maintained by taxonomyNames.pl to construct a series of
*.today files in this newAsm/ directory split up by the GenArk clade
categories.  This is in preparation for making the 'todo' listings
which will run the GenArk assembly build procedure.

##############################################################################
######## genbankToDo.sh and refseqToDo.sh ########

Run by the genbank.sh and refseq.sh scripts.  Taking the '*.today'
listing made by genbank.sh and refseq.sh to transform those
listings into a series of gb.todo.*.txt and rs.todo.*.txt files which are
the 'runBuild' commands that will build the GenArk assembly

These 'todo' files are accumlating in:
  /hive/data/outside/ncbi/genomes/reports/newAsm/
and where these scripts are located and running.

##############################################################################
######## primates.pl - obsolete ########

Script run by updateLists.sh to make a page for the genomewiki to
indicate primates assembly ready status.  This information is no
longer used.

##############################################################################
######## allVerts.sh - obsolete ########

Similar to primates.pl - makes wiki status pages for the state
of rediness of these assemblies.  Obsolete, no longer used
It happens to run:
  primates.pl
  mammals.pl
  aves.pl
  fish.pl
  vertebrates.pl
  which are all very similar

##############################################################################
######## cladesToday.sh ########

Run by updateLists.sh

Uses the files made by genbank.sh/refseq.sh to creaate
a single file listing all assembly identifiers and their
associated GenArk clade:
   /hive/data/outside/ncbi/genomes/reports/newAsm/asmId.clade.tsv

##############################################################################
######## cronUpdates.sh ########

Run by updateLists.sh in:

  /hive/data/outside/ncbi/genomes/reports/allCommonNames/cronUpdate.sh

This is going to run two different cluster jobs:
  .../reports/allCommonNames/genBank/rerunKu.sh
  .../reports/allCommonNames/refSeq/rerunHgwdev.sh

Ends up with a single file: asmId.commonName.all.txt
with all common names for every NCBI assembly (over 3 million)

##############################################################################
######## allCommonNames/genBank/rerunKu.sh ########

Run from cronUpdates.sh to run a cluster job on the ku cluster
to work up all the GenBank/GCA common names.  Creates the jobList
and calls kuBatch.sh via ssh to ku cluster to run this batch.
The 'runOne' script that is each cluster job uses commonNames.py
to scan the NCBI assembly_report.txt files in each NCBI mirrored
assembly directory.

##############################################################################
######## allCommonNames/genBank/kuBatch.sh ########

Run by allCommonNames/genBank/rerunKu.sh via an ssh to the ku cluster
node.  It is simply running the parasol batch prepared here
by rerunKu.sh.

##############################################################################
######## allCommonNames/refSeq/rerunHgwdev.sh ########

Similar to rerunKu.sh above but doesn't need an ssh to hgwdev cluster hub
since it is already on this machine.  Prepares the cluster jobList
and runs the batch on the hgwdev node.  The 'runOne' script which
is each cluster job uses commonNames.py to extract the common
names from the NCBI assembly_report.txt file for each NCBI assembly.

##############################################################################
##############################################################################

These processes have created the files that are needed by
the 'otto' process which rebuilds the hgcentral.genark table
and the hgcentral.assemblyList table.

