#!/bin/tcsh -efx

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
    echo "Read this script before executing"
    exit

#Set the human rat and mouse genome directories
     set h = /cluster/store5/gs.16/build33
     set m = /cluster/store2/mm.2003.02/mm3
     set r = /cluster/store4/rn2

#Set short symbolic names for the various genomes
     set hg = hg15
     set mm = mm3
     set rn = rn2

#Set the pairwise alignment directories
     set hm = $h/bed/blastz.mm3.2003-04-12-03-MS/axtBest
     set mr = $m/bed/blastz.rn2.2003-03-07-ASH/axtBest

#Set the output directory
     set hmr = /cluster/store5/multiz.hmr.3
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
   stageMultiz $h/chrom.sizes $m/chrom.sizes $r/chrom.sizes $i $mr -hPrefix=$hg. -mPrefix=$mm. -rPrefix=$rn. $hmr/$i:r
   echo done $i
 end

#Set up parasol directory to do multiz run proper.
 cd $hmr
 mkdir run1
 cd run1

#List staged directories one per line.
 echo ../chr*/* | wordLine stdin > in.lst

#create little script to run blastz.
cat > doMultiz << endDoMultiz
#!/bin/csh -ef
multiz \$1 - \$2 > \$3
endDoMultiz
chmod a+x doMultiz

#create gsub file with command to run
cat > gsub << endGsub
#LOOP
doMultiz \$(path1)/hm.maf \$(path1)/mr.maf {check out line \$(path1)/hmr.maf}
#ENDLOOP
endGsub
 
#create a spec file with one line per stage directory
 gensub2 in.lst single gsub spec

#Run parasol on the little cluster
 ssh kkr1u00 "cd $hmr/run1; para make spec"
 
#Concatenate the output into one file per chromosome
cd $hmr
mkdir hmr
foreach c (chr?{,?} chr*_random)
    mafFilter $c/$c.?{,??????,???????,????????}/hmr.maf > hmr/$c.maf
    echo done $c
end

#Clean up
rm -r chr?{,?} chr*_random

   

