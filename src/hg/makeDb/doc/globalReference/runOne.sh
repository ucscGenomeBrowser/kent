#!/bin/bash

# download, sequence, assemblyGap, gatewayPage, cytoBand, gc5Base,
# repeatMasker, simpleRepeat, allGaps, idKeys, windowMasker, addMask,
# gapOverlap, tandemDups, cpgIslands, ncbiGene, xenoRefGene, augustus,
# trackDb, cleanup

for asmIdName in $1
do
  gcDir=`echo $asmIdName | cut -c1-3`
  dir1=`echo $asmIdName | cut -c5-7`
  dir2=`echo $asmIdName | cut -c8-10`
  dir3=`echo $asmIdName | cut -c11-13`
  asmId="$gcDir_""`echo $asmIdName | cut -d'_' -f 2`"

  export genbankRefseq="genbank"
  export sciName=`grep -h "${asmId}" genBank.list | cut -f8 | sed -e 's/ /_/g;'`
  if [ "${gcDir}" = "GCF" ]; then
    genbankRefseq="refseq"
    sciName=`grep -h "${asmId}"  refSeq.list | cut -f8 | sed -e 's/ /_/g;'`
  fi

  # export stepStart="gatewayPage"
  # export stepEnd="addMask"
  # export stepStart="download"
  export stepStart="trackDb"
  export stepEnd="trackDb"
  export augustusSpecies="human"

  export buildDir="/hive/data/genomes/asmHubs/globalReference/${asmIdName}"
  export sourceDir="/hive/data/genomes/asmHubs/ncbiSrc/${gcDir}/${dir1}/${dir2}/${dir3}/${asmIdName}"
  # cluster specifications and ucscNames request
  export hubSpecs="-bigClusterHub=ku -smallClusterHub=hgwdev-101 -ucscNames"
  mkdir -p "${buildDir}"
  printf "###########################################################\n" >> ${buildDir}/${asmIdName}.${stepStart}-${stepEnd}.log
  printf "### %s\n" "`date '+%F %T'`" >> ${buildDir}/${asmIdName}.${stepStart}-${stepEnd}.log

  echo "~/kent/src/hg/utils/automation/doAssemblyHub.pl genbankRefseq
    vertebrate_mammalian \"${sciName}\" \"${asmIdName}\" -verbose=2
     -continue=$stepStart -stop=$stepEnd ${hubSpecs} -fileServer=hgwdev
      -augustusSpecies=${augustusSpecies} -buildDir=\"${buildDir}\"
        -sourceDir=\"${sourceDir}\"
" >> ${buildDir}/${asmIdName}.${stepStart}-${stepEnd}.log

  time (~/kent/src/hg/utils/automation/doAssemblyHub.pl genbankRefseq \
    vertebrate_other "${sciName}" "${asmIdName}" -verbose=2 \
     -continue=$stepStart -stop=$stepEnd ${hubSpecs} -fileServer=hgwdev \
      -augustusSpecies=${augustusSpecies} -buildDir="${buildDir}" \
        -sourceDir="${sourceDir}") \
          >> ${buildDir}/${asmIdName}.${stepStart}-${stepEnd}.log 2>&1 &

  echo ${buildDir}/${asmIdName}.${stepStart}-${stepEnd}.log
  printf "###########################################################\n" >> ${buildDir}/${asmIdName}.${stepStart}-${stepEnd}.log
done

wait

exit $?

  time (~/kent/src/hg/utils/automation/doAssemblyHub.pl genbankRefseq \
    vertebrate_other "${sciName}" "${asmIdName}" -verbose=2 \
     -continue=$stepStart -stop=$stepEnd ${hubSpecs} -fileServer=hgwdev \
      -augustusSpecies=${augustusSpecies} -buildDir="${buildDir}" \
        -sourceDir="${sourceDir}") \
   >> ${buildDir}/${asmIdName}.${stepStart}-${stepEnd}.log 2>&1

   echo ${buildDir}/${asmIdName}.${stepStart}-${stepEnd}.log

done

exit $?

# export asmIdName="GCA_900324485.2_fMasArm1.2"
export asmIdName="GCF_901001135.1_aRhiBiv1.1"



exit $?

    -buildDir dir     Construct assembly hub in dir instead of default
       $HgAutomate::clusterData/asmHubs/{genbank|refseq}/subGroup/species/asmId/
    -sourceDir dir    Find assembly in dir instead of default
                          $sourceDir
                          the assembly is found at:



usage: doAssemblyHub.pl [options] genbank|refseq subGroup species asmId
required arguments:
    genbank|refseq - specify either genbank or refseq hierarchy source
    subGroup       - specify subGroup at NCBI FTP site, examples:
                   - vertebrate_mammalian vertebrate_other plant etc...
    species        - species directory at NCBI FTP site, examples:
                   - Homo_sapiens Mus_musculus etc...
    asmId          - assembly identifier at NCBI FTP site, examples:
                   - GCF_000001405.32_GRCh38.p6 GCF_000001635.24_GRCm38.p4 etc..

options:
    -continue step        Pick up at the step where a previous run left off
                          (some debugging and cleanup may be necessary first).
                          step must be one of the following:
                          download, sequence, assemblyGap, gatewayPage, gc5Base, repeatMasker, simpleRepeat, allGaps, idKeys, addMask, windowMasker, cpgIslands, augustus, trackDb, cleanup
    -stop step            Stop after completing the specified step.
                          (same possible values as for -continue above)
    -buildDir dir     Construct assembly hub in dir instead of default
       /hive/data/genomes/asmHubs/{genbank|refseq}/subGroup/species/asmId/
    -sourceDir dir    Find assembly in dir instead of default
                          /hive/data/outside/ncbi/genomes
                          the assembly is found at:
  sourceDir/{genbank|refseq}/subGroup/species/all_assembly_versions/asmId/
    -ucscNames        Translate NCBI/INSDC/RefSeq names to UCSC names
                      default is to use the given NCBI/INSDC/RefSeq names
    -bigClusterHub mach   Use mach (default: ku) as parasol hub
                          for cluster runs with very large job counts.
    -dbHost mach          Use mach (default: hgwdev) as database server.
    -fileServer mach      Use mach (default: fileServer of the build directory)
                          for I/O-intensive steps.
    -smallClusterHub mach Use mach (default: ku) as parasol hub
                          for cluster runs with smallish job counts.
    -workhorse machine    Use machine (default: hgwdev) for compute or
                          memory-intensive steps.
    -debug                Don't actually run commands, just display them.
    -verbose num          Set verbose level to num (default 1).
    -help                 Show detailed help and exit.

Automates build of assembly hub.  Steps:
    download: sets up sym link working hierarchy from already mirrored
                files from NCBI in:
                      /hive/data/outside/ncbi/genomes/{genbank|refseq}/
    sequence: establish AGP and 2bit file from NCBI directory
    assemblyGap: create assembly and gap bigBed files and indexes
                 for assembly track names
    gatewayPage: create html/asmId.description.html contents
    gc5Base: create bigWig file for gc5Base track
    repeatMasker: run repeat masker cluster run and create bigBed files for
                  the composite track categories of repeats
    simpleRepeat: run trf cluster run and create bigBed file for simple repeats
    allGaps: calculate all actual real gaps due to N's in sequence, can be
                  more than were specified in the AGP file
    idKeys: calculate md5sum for each sequence in the assembly to be used to
            find identical sequences in similar assemblies
    addMask: combine repeatMasker and trf simpleRepeats into one 2bit file
    windowMasker: run windowMasker cluster run, create windowMasker bigBed file
                  and compute intersection with repeatMasker results
    cpgIslands: run CpG islands cluster runs for both masked and unmasked
                sequences and create bigBed files for this composite track
    trackDb: create trackDb.txt file for assembly hub to include all constructed
             bigBed and bigWig tracks
    cleanup: Removes or compresses intermediate files.
All operations are performed in the build directory which is
/hive/data/genomes/$db/bed/template.$date unless -buildDir is given.


