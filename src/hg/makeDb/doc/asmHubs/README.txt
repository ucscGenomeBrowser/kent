#############################################################################
# Update 2025-11-07
#############################################################################

Since the orderList.tsv listings have become very large, it has
become inconveniently time consuming to run through the standard
process as described below.  There is a newer 'minimal build' procedure
in use now that can save a lot of time.

At the point where you have added your new assembly to the orderList.tsv
file, instead of running through the full sequence of make commands
for the entire clade set you are working on, go to the clade directory and
run:

   time (../asmHubs/minimalBuild.sh) > minimal.log 2>&1

and that script will run the make commands on only the new assemblies
in the orderList.tsv.  It still takes a long time (hours) to do part of that
job when it is making up the clade index.html files, but much of the
time consuming working through every assembly in the list is eliminated.

#############################################################################
### Building the GenArk assembly hubs ###
#############################################################################
### Requests from the request system:

When a user sends in a request with an accession ID, e.g.: GCF_002776525.5
the assembly may already exist in some version.  To check if something
already exists, use just the number part of the ID: 002776525 and
check the existing listings in the source tree:

    grep 002776525 ~/kent/src/hg/makeDb/doc/*AsmHub/*.tsv

They may be asking for a newer version, or they may be asking for
a GenBank version when a RefSeq version already exists.  Can decide
if what we have is better than what they ask for.  True, sometimes they
may want a specific older version, negotiate that with the user in
email to see if they would accept a newer version.

When not there, check to see if has been recognized as something
to build.  Again, just the number, for RefSeq assemblies:

    grep 002776525 /hive/data/outside/ncbi/genomes/reports/newAsm/rs.todo.*
for Genbank:
    grep 002776525 /hive/data/outside/ncbi/genomes/reports/newAsm/gb.todo.*
decide which one is best or most up to date, RefSeq is always first choice.
Those are the pre-ready to go build commands to be run in the allBuild
directory:

    cd /hive/data/genomes/asmHubs/allBuild
    time (./runBuild GCA_002776525.5_ASM277652v5 primates Piliocolobus_tephrosceles) >> GCA_002776525.5.log 2>&1

If it doesn't show up there, check the 'master' listings from NCBI:

   /hive/data/outside/ncbi/genomes/reports/assembly_summary*.txt
assembly_summary_genbank.txt             assembly_summary_refseq.txt
assembly_summary_genbank_historical.txt  assembly_summary_refseq_historical.txt

These asssembly_summary*.txt files can also be scanned for scientific names
if that is all the user supplied.

#############################################################################
###
###  To build a single hub:
#############################################################################

0: Given an accession identifier, e.g. GCF_002776525.5
a. find build command and designated clade (see also above discussion)
b. run the build of the hub (see also above discussion)
c. add lines to source tree files master.run.list and
                      doc/<clade>AsmHug/<clade>.orderList.tsv
d. in the source tree doc/<clade>AsmHub/ directory, running commands:
e. make symLinks      # prepares staging directory with symlinks to the build
f. then: time (make) > dbg 2>&1 # check for errors: egrep "miss|err" dbg
g. time (make verifyTestDownload) >> test.down.log 2>&1 # check for errors
                                                   # grep check test.down.log
h. time (make sendDownload) >> send.down.log 2>&1 # check for errors
                                                  # grep error send.down.log
i. time (make verifyDownload) >> verify.down.log 2>&1 # check for errors
                                                  # grep check verify.down.log
j. verify the browser functions: https://genome.ucsc.edu/h/GCF_002776525.5

### Details of those steps:

1.  Given an accession identifier, e.g. GCF_002776525.5
    Find the build command and designated clade:

  grep GCF_002776525.5 \
     /hive/data/outside/ncbi/genomes/reports/newAsm/{rs,gb}.todo.*.txt

Answer: 'primates' clade and command:
rs.todo.primates.txt:./runBuild GCF_002776525.5_ASM277652v5 primates Piliocolobus_tephrosceles   2019_12_12

    If that grep finds nothing, that browser may already be built.
    Can grep source tree file: ~/kent/src/hg/makeDb/doc/asmHubs/master.run.list
    for your accession to see if it already done

2.  Run the build of the browser in the directory:
    cd /hive/data/genomes/asmHubs/allBuild
time (./runBuild GCF_002776525.5_ASM277652v5 primates Piliocolobus_tephrosceles) > GCF_002776525.5.log 2>&1 &
    That could take several days for a large genome, a few hours for a small one
    When it is done, there will be a asmId.trackDb.txt file in the build
    directory:
/hive/data/genomes/asmHubs/refseqBuild/GCF/002/776/525/GCF_002776525.5_ASM277652v5/

3.  When the build is done, add that runBuild command to the source tree:
             ~/kent/src/hg/makeDb/doc/asmHubs/master.run.list
    maintain the sorted order of that file

4.  Add the full assembly ID and common name to the primates.orderList.txt
    cd ~/kent/src/hg/makeDb/doc/primatesAsmHub
    echo GCF_002776525.5_ASM277652v5 | ../asmHubs/commonNames.pl /dev/stdin
 GCF_002776525.5_ASM277652v5     Ugandan red Colobus (RC106 2019)
    Keep the list in order by the second column case insensitive.
    These common names will be the pull-down menu list in the browser
    to select a genome from this group.  Make the common name unique so
    there is something the user can see that they can identify as the
    assembly they want to use.
    Extra credit:  if your new build is an updated version of
                   that genome assembly, move the old one out of this
                   orderList.txt into ../legacyAsmHub/legacy.orderList.txt
                   Same procedures there to push out that group.

5. Prepare the build for the push.  In this primatesAsmHub directory:
     time (make) > dbg 2>&1
     This could stop prematurely if errors are encountered, to verify
     when done, check for errors: egrep -i "error|fail|missing|cannot" dbg
     should be nothing significant

6. Verify the browser is correct on hgwdev:
     time (makeVerifyTestDownload) >> test.down.log 2>&1
     should finish with an all clear line, no failures:
# checked  58 hubs, 58 success, 0 fail, total tracks: 1188, 2023-02-15 13:48:07

7. Push the hub to hgdownload (and dynamic blat server):
     time (make sendDownload) >> send.down.log 2>&1
     should stop if there are errors.  Can verify:
         egrep -i "error|fail|missing|cannot" send.down.log

8. Verify the hub is correctly on hgdownload:
     time (make verifyDownload) >> verify.down.log 2>&1
     should finish with an all clear line, no failures:
# checked 58 hubs, 58 success, 0 fail, total tracks: 1188, 2023-02-15 13:58:02

9. Verify the hub appears in the browser:
        https://genome.ucsc.edu/h/GCF_002776525.5

Extra historical discussion included below.

#############################################################################
#############################################################################
### see below for adding custom/local developed tracks to an existing GenArk hub
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

  time (./runBuild GCF_000001405.39_GRCh38.p13 primates Homo_sapiens) >> GCF_000001405.39.log 2>&1 &

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
### adding custom/local developed tracks to a GenArk hub
#############################################################################

Work in the trackData/ directory of the assembly hub in a directory
name of the track, think of this as your /hive/data/genomes/<db>/bed/myTrack/
usual work directory as if it were a database assembly.

For example, the extra pcrAmplicon track on the Monkeypox browser
GCF_000857045.1_ViralProj15142

Is developed in:
/hive/data/genomes/asmHubs/refseqBuild/GCF/014/621/545/GCF_014621545.1_ASM1462154v1/trackData/pcrAmplicon/

When your data is ready, add your big* files, ixIxx and html page description
files to the browser with symLinks in the bbi, ixIxx and html directories:

/hive/data/genomes/asmHubs/refseqBuild/GCF/014/621/545/GCF_014621545.1_ASM1462154v1/bbi/
and
/hive/data/genomes/asmHubs/refseqBuild/GCF/014/621/545/GCF_014621545.1_ASM1462154v1/ixIxx/
/hive/data/genomes/asmHubs/refseqBuild/GCF/014/621/545/GCF_014621545.1_ASM1462154v1/html/

To get your track added to the GenArk hub, place your trackDb.txt
definitions in the special named file: <asmId>.userTrackDb.txt
in the top-level build directory:
/hive/data/genomes/asmHubs/refseqBuild/GCF/014/621/545/GCF_014621545.1_ASM1462154v1/
for example: GCF_014621545.1_ASM1462154v1.userTrackDb.txt

Your track will push out to hgdownload with this GenArk hub the next time
the build is run for the clade this organism is packaged in.

Typical 'build' sequence to do the release of a clade set:

  cd ~/kent/src/hg/makeDb/doc/viralAsmHub
  # builds symLinks for delivery staging directory, constructs index pages
  # for this clade set, makes everything available on genome-test
  time (make) >> dbg 2>&1
  # when finished, examine the dbg file to see if there are any errors reported
  # by the scripts.  Then, verify it is looking good in the staging
  # directory on genome-test:
  time (make verifyTestDownload) >> test.down.log 2>&1
  # this testing is performed by the API on hgwdev.
  # this test.down.log file accumulates each time a build is run, to make sure
  # it is sane and there are no errors, grep for 'checked' to see lines such as:

  grep checked test.down.log
# checked 221 hubs, 221 success, 0 fail, total tracks: 4720, 2022-09-25 14:58:55
# checked 222 hubs, 222 success, 0 fail, total tracks: 4740, 2022-10-04 11:55:28

  # if you wanted to view this clade set on genome-test to see what it
  # looks like, the URL is:  https://genome-test.gi.ucsc.edu/hubs/viral/
  # each clade has a different directory here:
  #  primates mammals birds fish vertebrate plants fungi viral bacteria archaea

  # if it looks good on genome-test and verifyTestDownload runs without errors,
  # the hub can push to hgdownload:

  time (make sendDownload) >> send.down.log 2>&1

  # there isn't much to see in this send.down.log, it is just for the record
  # then to verify it is correct on hgdownload:

  time (make verifyDownload) >> verify.down.log 2>&1 &
  # this testing runs via the API on hgwbeta so that the access
  # activity logs on the RR won't be disturbed by such testing.

  # to see if it is sane, grep for 'checked' in this log file:
  grep checked verify.down.log

# checked 221 hubs, 221 success, 0 fail, total tracks: 4720, 2022-09-25 19:42:48
# checked 222 hubs, 222 success, 0 fail, total tracks: 4740, 2022-10-04 12:23:35

#############################################################################
