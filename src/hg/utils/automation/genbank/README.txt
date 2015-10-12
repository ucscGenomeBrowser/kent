
UCSC procedure to mirror genbank/refseq assemblies and generate assembly hubs

This README.txt file is from:

http://genome-source.cse.ucsc.edu/gitweb/?p=kent.git;a=blob_plain;f=src/hg/utils/automation/genbank/README.txt

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
