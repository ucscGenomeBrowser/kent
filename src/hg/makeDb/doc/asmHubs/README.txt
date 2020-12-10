#############################################################################
### Building the assembly hubs ###
#############################################################################

The build of each assembly takes place in, for example:

  /hive/data/genomes/asmHubs/refseqBuild/GCF/000/001/405/GCF_000001405.39_GRCh38.p13/

(There is a corresponding hierarchy for 'genbank' GCA assemblies, i.e.:

/hive/data/genomes/asmHubs/genbankBuild/GCA/902/686/455/GCA_902686455.1_mSciVul1.1

)

I have a 'goto' function in my shell, you can view at:
  ~hiram/.bashrc.hiram
which I use to move around in this spread out hierarchy.  For example:

  $ goto GCF_000001405

will get you to that build directory (when there is only one GCF_000001405)

You should construct any new files in this directory hierarchy.  Maybe
a subdirectory here if you have a whole category of files,  Note
subdirectories already here: bbi html ixIxx for example.

(download, sequence, idKeys, trackData are directories for data construction
  during the build)

To deliver files from this build to hgdownload, scripts in:
   ~/kent/src/hg/makeDb/doc/asmHubs/
construct symlinks from the build directory into the delivery staging directory hierarchy, for example:
   /hive/data/genomes/asmHubs/GCF/000/001/405/GCF_000001405.39/

Nothing but symlinks here and just the deliver files for hgdownload:
  ls -ogLd *
-rw-rw-r-- 1  945974069 Sep 10  2019 GCF_000001405.39.2bit
-rw-rw-r-- 1     888417 Sep 10  2019 GCF_000001405.39.agp.gz
-rw-rw-r-- 1      18097 Sep 10  2019 GCF_000001405.39.chrom.sizes.txt
-rw-rw-r-- 1      29424 Sep 25 11:58 GCF_000001405.39.chromAlias.txt
-rw-rw-r-- 1 2915673072 Jul 16 15:16 GCF_000001405.39.trans.gfidx
-rw-rw-r-- 1 2262217236 Jul 16 15:19 GCF_000001405.39.untrans.gfidx
drwxrwxr-x 2       4096 Sep 23 15:33 bbi
-rw-rw-r-- 1        354 Dec  1 12:16 genomes.txt
-rw-rw-r-- 1        508 Dec  1 12:16 groups.txt
drwxrwxr-x 2       4096 Dec  1 12:16 html
-rw-rw-r-- 1        240 Dec  1 12:16 hub.txt
drwxrwxr-x 2       4096 Sep 23 15:33 ixIxx
-rw-rw-r-- 1       9910 Sep 23 15:33 trackDb.txt

Note how the names become shorter here, losing the full assembly identifier.
Don't need that.  There should be only one 'GCF_000001405.39' assembly.
NCBI has made a couple of mistakes and these names became duplicated for
a couple of assemblies.  Don't care about that.  Eliminated the garbage.

So, to add the construction of the deliver symlinks for your new files,
you would add something to: ~/kent/src/hg/makeDb/doc/asmHubs/mkSymLinks.pl
This is assuming you do want to deliver these files to hgdownload.  I would
guess you would since external users that want to copy this assembly can copy
this directory from hgdownload to get everything they need to operate it
independently from us.

You don't operate the scripts in .../makeDb/doc/asmHubs/ by themselves.
The are used from makefile rules in each assembly hub definition directory.
For example, for the primates, in the directory:
   kent/src/hg/makeDb/doc/primatesAsmHub/
you just type 'make' and it does everything to get these items ready for
delivery.  This is what makes the symLinks and all other files to make
the assembly hub function.

(This is *not* the build of the files in the build hierarchy, see below)

Other hub dirctories here in makeDb/doc/

primatesAsmHub
mammalsAsmHub
birdsAsmHub
fishAsmHub
vertebrateAsmHub
legacyAsmHub
plantsAsmHub
bacteriaAsmHub
vgpAsmHub

Future work will create:

fungiAsmHub
invertebrateAsmHub
viralAsmHub
protozoaAsmHub
bacteriaAsmHub
archaeaAsmHub

#############################################################################
### To run up a build of an assembly ###
#############################################################################

The actual build is taking place with the help of the 'runBuild'
script (copy here in ~/kent/src/hg/makeDb/doc/asmHubs/runBuild)

The builds are operated from the directory:

   /hive/data/genomes/asmHubs/allBuild/
   (a location to accumulate log files, and run lists, thus work history)

The 'runBuild' is operated, for example, a single assembly:

  time (./runBuild GCF_000001405.39 GCF_000001405.39_GRCh38.p13 vertebrate_mammalian Homo_sapiens) >> GCF_000001405.39.log 2>&1 &

Or, typically, there may be a whole list of such commands
( such as in the master.run.list here:
    ~/kent/src/hg/makeDb/doc/asmHubs/master.run.list
)

These are run, for example 5 at a time:
  time (kent/src/hg/utils/automation/perlPara.pl 5 master.run.list) \
     >> bigRun.log 2>&1

The 'runBuild' script is usually set up to run all steps from
'download' to 'trackDb', and it is OK to use it like this even on
a build that has already taken place (currently it is disabled to
avoid trying to rebuild an assembly).  There are cases, for example,
where I want to update all the trackDb files since something has
been improved for trackDb, in which case I adjust the
stepStart and stepEnd to run just the trackDb step.  (would have
to disable the rebuild prevention)

#############################################################################
