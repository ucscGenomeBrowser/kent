#!/bin/csh

#Here's a script to to create this directory for the
#the human/mouse/rat three way alignments.  It assumes
#you're already generated axtBests for the mouse/rat
#and human/mouse pairwise blastz alignments.  This
#script needs to be run on a machine with 4 gig of RAM
#and it is fairly i/o intensive.


#Set the following variables to point to the output
#and input for the appropriate assembly.

#Copy this script and take out this line, which is just
#to help make sure you do set the variables
    echo "Read this script before executing
    exit

#Set the human rat and mouse genome directories
     set h = /cluster/store4/gs.14/build31
     set m = /cluster/store2/mm.2002.02/mm2
     set r = /cluster/store4/rn1

#Set short symbolic names for the various genomes
     set hg = hg13
     set mm = mm2
     set rn = rn1

#Set the pairwise alignment directories
     set hm = $h/bed/blastz.mm2.2002-12-5-ASH/axtBest
     set mr = $m/bed/blastz.rn1.2003-01-09-ASH/axtBest

#Set the output directory
     set hmr = /cluster/store2/multiz.hmr.1
     mkdir $hmr

#Create indexes for the mouse/rat axtBest files as so:

 cd $mr
 foreach i (*.axt)
     axtIndex $i $i.ix
     echo indexed $i
 end
     
#Run stageMultiz as so:
 cd $hm
 foreach i (*.axt)
   stageMultiz $h/chrom.sizes $m/chrom.sizes $r/chrom.sizes $i $mr -hPrefix=$hg -mPrefix=$mm -rPrefix=$rn $hmr/$i:r
   echo done $i
 end

