UniProt mapping pipeline, Max 2016/2017, updates in 2021

Updates the UniProt tracks from UniProt.org, which puts out a new release every
month. See http://www.uniprot.org/news/

Two copies of these scripts exist. Edit the one in the kent tree,
src/hg/utils/otto/uniprot, commit, then "make install" to push it out. The one
cron runs is /hive/data/outside/otto/uniprot. "make diff" lists files that have
drifted apart, worth checking now and then: the rsync in "make install" uses -u
and will not overwrite a live file that is newer than the tree.

How it is started:

Cron, from otto's crontab, on the 26th of the month:

    00 07 26 * * /hive/data/outside/otto/uniprot/doUpdate.sh run

doUpdate.sh activates venv/, checks that the parser can start, runs doUniprot and
reports the outcome. To start a run by hand, use doUpdate.sh, not doUniprot, so
the environment and the logging are the same as under cron.

Python environment:

uniprotToTab needs the lxml XML parser. It is not in the python standard library
and hgwdev has no system-wide copy, so it lives in a virtualenv in venv/. Build
or rebuild it with:

    cd /hive/data/outside/otto/uniprot && ./makeVenv.sh

Note that ~/.local is not enough: cron runs this as otto, which does not see
anyone else's per-user python packages.

Did it run?

runLog.txt gets one line per run and is never truncated, so it is the history of
the job:

    START      a run began
    NOCHANGE   UniProt had no new release, nothing to do, no mail sent
    OK         new release, tracks rebuilt
    FAIL       the run died, exit code and log named on the line

lastRun.log is the log of the most recent run and is overwritten every month. A
failing run is kept as lastFail.log, and its last 25 lines are mailed to the
MAILTO addresses in otto's crontab. A month without a new UniProt release sends
no mail, so silence means "nothing to do", not "it worked".

version.txt in each bigBed/<db> directory is what the trackDb dataVersion setting
shows on the track description page. It is only rewritten when the release string
changes, so its date on disk is the date the data last moved, not the date the
pipeline last ran.

Directories:

fasta - current protein sequences and their sizes, named by taxon
geneMaps/ - a mapping of the current protein sequences to the genome, one for each species and database.
      Includes the md5 of the fasta files, so we do not have to recompute when sequences have not changed
      These files are used by pslMap.

      Also includes everything needed for a bigPsl file of these psls and the bigPsls themselves.

bigBed - one bigBed for every subTrack

Pipeline:

The main driver script is doUniprot. It requires the parameter "run" to do
anything. It goes through these steps:
- downloads UniProt XML with lftp. This takes 2-3 days. Skip this step with -l for development.
  [ It's hard to speed this up, as the EBI FTP server does not allow parallel connections, at the time
  of writing. ]
- converts it to tab-sep and fasta files using uniprotToTab. This takes 3-4 days! Skip this step with
  -p whenever you can for debugging or development.
  [ jIt is hard to speed this up, as there is only a single huge XML file, without an index. The EBI 
  has a pilot where they provide offsets into the XML but it's not a real produce yet. ]
- for each UniProt taxon ID, find the relevant UCSC db identifiers, adding a few manual overrides,
  e.g. 9606 always uses both hg19 and hg38. wuhCor1 is skipped, etc.
  You can limit the script to only certain dbs with e.g. --onlyDbs=hg19
  You can display the current mapping with 'doUniprot --db'
  This will also show the correct trackDb make command if you want to remake all trackDbs.
- for each assembly, try to guess a transcript gene track and find transcript
  sequences for it and finds or fakes a transcript.psl file for the transcripts.
  Supported transcript tracks are: ncbiRefSeq, ensembl, augustus.
  They are tried in this order. Hg19 is hardcoded to refGene because CSAG3 NM_001129826.3
  exists only on chrX_jh159150_fix in ncbiRefSeq, Terence confirmed this is an issue.
  hg38 is hardcoded to ncbiRefSeq. see findBestGeneTable()
- tries to create a UniProtId <-> transcriptId pairs table, if possible (from the UniProt xref fields)
  This massively reduces the false positives, for protein families with a lot of almost
  identical transcripts. There often a small mismatches between this table and
  the actual transcript set, so the file is cleaned up (and later pslSelect is run
  with -qPass, so any alignments that do not appear in the file just go through)
- aligns the UniProt fasta files against the transcript
  sequences with BLAST, using the script makeUniProtPsl.sh, on the cluster,
  which uses mapUniprot_doBlast for the cluster jobs.
  This can take 1-2 hours, even using the cluster.
  The output are PSL "lift" files for pslMap, one per assembly. The file names include
  the gene track MD5 and the pslSelect MD5 and the old results are reused, if the MD5s
  match (CPU-heavy BLAST alignment). See section below for more details.
  If the old mapping can be reused, a run of the pipeline takes < 1min per assembly.
- Writes the UniProt features to PSL files, then lifts those with pslMap to the
  genome using the lift files that were just created on the cluster
- Converts the resulting UniProt annotation PSLs to BED and then to bigBed
  The pslMap alignments are converted to bigPsl with various extra fields.
  These bigPsl files are split into SwissProt/Trembl, two subtracks, because 
  our filters don't work on bigPsl and also because the Trembl sequences on human
  are pretty useless anyways.
- If everything was successful for all assemblies:
  - link the new bigBeds into /gbdb/
  - create little version.txt files in every bigBed directory to indicate the UniProt release
  - copy the new files to /usr/local/apache/htdocs-hgdownload/goldenPath/archive/hg19/uniprot/<release>/
    and update the 'current' symlink there. Throw in the UniProt -> Genome PSL file.

Alignments:

The more complicated part is the mapping from UniProt to Genome.
It's handled by the shell script makeUniProtPsl.sh. The script makes various
assumptions that may need tweaking one day:
- it uses mapUniprot_doBlast (a TCL script! HT to MarkD) for the BLAST cluster jobs.
  Note that we use /cluster/bin/blast/x86_64/blast-2.2.16/bin/blastall, an older
  BLAST version, to align UniProt protein sequences against transcript DNA sequences with tblastn
- it uses various filters on the results, then pslMap's these to the genome through the transcript.psl.
- the minimum percent ID of the alignments is 95%, because not all proteins match at 100% to the transcripts
- it keeps only the top 1% of the alignments using pslCDnaFilter
- any transcripts on _hap/_alt/_fix sequences are removed, to avoid that annotations are "sucked away" from the main 
  chromosomes (should it use /hive/data/genomes/hg38/jkStuff/hg38.haplotypes.psl instead and the -hap option in pslCDnaFilter?)
- I am not sure how to speed up the alignment. NCBI suggests to change the chunking, and create
  e.g. one query file for 10 queries and one query file for 10 targets, and align only
  queries to known targets. Was too much work, so for now the BLAST runs are a bit slow.

