############################################################################

UCSC procedure to mirror genbank/refseq assemblies and generate assembly hubs

This README.txt file is from:

http://genome-source.soe.ucsc.edu/gitlist/kent.git/blob/master/src/hg/utils/automation/genbank/README.txt

############################################################################
# mirror genbank/refseq hierarchy

There is a cron job running once a day M-F that runs an rsync command
to fetch a select set of files from genbank/refseq.  The script here is:
  cronUpdate.sh
with one argument specifying either the refseq or genbank assembly
hierarchy, e.g.:
  cronUpdate.sh refseq

The accumulating hierarchy at UCSC is in:
   export outside="/hive/data/outside/ncbi/genomes/refseq"
Or, if genbank assemblies:
   export outside="/hive/data/outside/ncbi/genomes/genbank"

The rsync command has a number of include statements to pick up
a select set of files:

  cd "${outside}"
  rsync -avPL --include "*/" --include="chr2acc" --include="*chr2scaf" \
    --include "*placement.txt" --include "*definitions.txt" \
    --include "*_rm.out.gz" --include "*agp.gz" --include "*regions.txt" \
    --include "*_genomic.fna.gz" --include "*gff.gz" \
    --include "*_assembly_report.txt" --exclude "*" \
        rsync://ftp.ncbi.nlm.nih.gov/genomes/genbank/${D}/ ./${D}/ \
     || /bin/true

The "${D}" directory name is one of the directories
at ftp.ncbi.nlm.nih.gov/genomes/{refseq|genbank}/${D}/

Slightly different directory listings for refseq vs. genbank.
For refseq assemblies:
for D in archaea bacteria fungi invertebrate mitochondrion plant plasmid \
  plastid protozoa vertebrate_mammalian vertebrate_other viral

For genbank assemblies:
  for D in archaea bacteria fungi invertebrate other plant protozoa \
     vertebrate_mammalian vertebrate_other

The cron script also maintains file listings and logs in:
   ${outside}/fileListings/yyyy/mm/
   ${outside}/logs/yyyy/mm/
to track changes over time and provide a record of activity

############################################################################
# processing NCBI files into assembly hub files

Processing takes place in the two hierarchies:
   /hive/data/inside/ncbi/genomes/refseq/
   /hive/data/inside/ncbi/genomes/genbank/

The scripts from this source directory:
     kent/src/hg/utils/automation/genbank/
are used to process the NCBI files into assembly hub files.
This processing is done with a cluster computer set up since there
are a large number of steps to perform.

The primary driving script is: ncbiToUcsc.sh
It takes two arguments:
    1. genbank|refseq - to specify which hierarchy to work on
    2. <pathTo>/*_genomic.fna.gz - fasta sequence file in the 'outside' path

############################################################################
