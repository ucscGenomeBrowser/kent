
################################################################
### VGP project information
################################################################

For naming scheme if the assembly names:

 https://github.com/VGP/vgp-assembly/blob/master/VGP_specimen_naming_scheme.md


The one letter prefix [abfmrs] corresponds to one of:

prefix  class

a       amphibians
b       birds
f       fishes
m       mammal
r       reptiles
s       sharks and relatives

################################################################
###  Building the VGP assembly hubs
################################################################

Working in the directory hierarchy:

   /hive/data/genomes/asmHubs/VGP

The ucscNames directory contains the built assembly hubs:

   /hive/data/genomes/asmHubs/VGP/ucscNames

The building of each hub was managed from the directory:

   /hive/data/genomes/asmHubs/VGP/cheapParasol

Adjusting the script in that directory:

   /hive/data/genomes/asmHubs/VGP/cheapParasol/runOneJob

to run steps one at a time, or multiple steps to build
the hubs.  Using the list of jobs to run:

   /hive/data/genomes/asmHubs/VGP/cheapParasol/build.run.list

And giving that list to the perlPara.pl script to run five
of them at once and keep five jobs running until finished:

   time (./perlPara 5 build.run.list) > variousStep.log 2>&1

Broken steps were fixed up and run manually, one step
at a time.

   time (./runOneJob GCA_004027225.1_bStrHab1_v1.p) > GCA_004027225.1_bStrHab1_v1.p.log 2>&1

The runOneJob script writes a shell script for the steps it
is running into the work directory:

   VGP/ucscNames/<asmId>/steps.<stepStart>-<stepEnd>.sh

And when that script is run, it leaves a log file in that
work directory:

   VGP/ucscNames/<asmId>/<asmId>.<stepStart>-<stepEnd>.log

################################################################
###  Generating index pages for the VGP assembly hubs
################################################################

After the hubs were finished building successfully,
The scripts in VGP/ucscNames  build the necessary
index.html pages and the genomes.txt file:

./mkHubIndex.pl > index.html
chmod 775 index.html

./mkAsmStats.pl > asmStatsVGP.html
chmod 775 asmStatsVGP.html

./mkGenomes.pl > genomes.txt

The 'chmod 775' allow those .html pages to run their 'include'
commands.

################################################################
###  adjusting the common names
################################################################

Some of the common names from the assembly report files were
not specific enough.  The 'commonNames.txt' file was used
to fix up the imprecise names:

GCF_900634415.1_fCotGob3.1      Channel bull blenny
GCA_901765095.1_aMicUni1.1      tiny Cayenne caecilian
GCA_901699155.1_bStrTur1.1      European turtle dove

The scripts use that file to correct the names.

The hub.txt file is:

hub VGP
shortLabel VGP
longLabel Vertebrate Genome Project assemblies
genomesFile genomes.txt
email hclawson@ucsc.edu
descriptionUrl index.html

