#!/bin/csh

set scriptDir="/projects/compbio/usr/weber/kent/src/hg/sampleTracks/bin/"

#track specific values
alias foreachChrom 'foreach f (1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 X)'
set baseDir="/cluster/store5/weber/rat/"
set target="mm3"
set query="Rat"
set queryTable="blastzBestRat"
set axtDir="/cluster/store2/mm.2003.02/mm3/bed/blastz.rn2.2003-03-07-ASH/axtBest/"
set outTable="mm3Rn2L"

mkdir $baseDir
cd $baseDir
#create table of the ancient repeats:
foreachChrom
	echo 'create table chr'${f}'_arTmp select * from chr'${f}'_rmsk, ancientRepeat where repName=name and repClass=family and repFamily=class;'
end
featureBits $target arTmp $queryTable -bed=aar${target}${query}.bed

#- Generate background counts with windows that have a 6kb counts,
#      with a maximum windows size of 512kb and sliding the windows by
        foreach axt (${axtDir}chr*.axt)
           set chr=$axt:t:r
           set tab=$chr.6kb-aar.cnts
           hgCountAlign -selectBed=aar${target}${query}.bed -winSize=512000 -winSlide=1000 -fixedNumCounts=6000 -countCoords $axt $tab
        end

#    - Generate counts for AARs with 50b windows, slide by 50b
        foreach axt (${axtDir}chr*.axt)
           set chr=$axt:t:r
           set tab=$chr.50b-aar.cnts
           hgCountAlign -selectBed=aar${target}${query}.bed -winSize=50 -winSlide=50b $axt $tab
        end


#    - Generate counts for all with 50b windows, slide by 50b
        if( -e countJobs ) rm countJobs
        foreach axt (${axtDir}chr*.axt)
           set chr=$axt:t:r
           set tab=$chr.50b.cnts
           echo ${scriptDir}hgCountAlign.csh -winSize=50 -winSlide=50b $axt $tab >> countJobs
        end

#    - Generate counts for all with 50b windows, slide by 5b
        foreach axt (${axtDir}chr*.axt)
           set chr=$axt:t:r
           set tab=$chr.50b-slide5.cnts
           echo ${scriptDir}hgCountAlign.csh -winSize=50 -winSlide=5b $axt $tab >> countJobs
        end
        para make countJobs

      rm *random*

      mkdir 50b-aar/  50b-slide5/  50b-slide50/  6kb/
      mv *6kb-aar.cnts 6kb/
      mv *-aar.cnts 50b-aar/
      mv *.50b.cnts 50b-slide50/
      mv *.50b-slide5.cnts 50b-slide5/

      #Run sumcounts.csh to get parameters for semiNormJob.csh
      cd 50b-aar/
      ${scriptDir}sumCounts.csh > sumOut
      set totalBases = `awk '{print($2)}' sumOut`
      set percentId = `awk '{print($3)}' sumOut`
      cd ../

      if( -e semiNormJobs ) rm semiNormJobs
      foreach f (50b-slide50/*.cnts 50b-slide5/*.cnts 50b-aar/*.cnts)
      echo ${scriptDir}semiNormJob.csh 6kb/$f:t:r:r.6kb-aar.cnts $f $totalBases $percentId >> semiNormJobs
      end
      para make semiNormJobs

      cat *aar.s.txt > all
      mv all 50b-win+50b-slide_aar

      cat *.50b-slide5.s.txt > all
      mv all 50b-win+5b-slide_all


      cat *.50b.s.txt > all
      mv all 50b-win+50b-slide_all

              
                      
#get range of all s-scores
${scriptDir}removeSmallWinsLocal.csh 50b-win+50b-slide_aar 15 > 50b-win+50b-slide_aar.atleast15
${scriptDir}minMax4s.csh 50b-win+50b-slide_aar.atleast15 > mout
foreach f (*.50b-slide5.s.txt)
    ${scriptDir}removeSmallWinsLocal.csh $f 15 > $f.atleast15
    ${scriptDir}minMax4s.csh $f.atleast15 >> mout
end

#make track neutral density (10,000 points not 100,000) 
#(and with range from overlapping (slide5) window s-scores)
${scriptDir}only4.csh 50b-win+50b-slide_aar.atleast15 > rin
R --vanilla < ${scriptDir}rscript > rscript.out
mv rout denneut10000.xy


#compute L-scores
if( -e sJobs ) rm sJobs
foreach f (*.s.txt.atleast15)
    echo ${scriptDir}'cdf.csh denneut10000.xy '$f >> sJobs
end
para make sJobs

#dump queryTable hits and create file of just start stops (for the target)
mkdir $queryTable
chmod 777 $queryTable
foreachChrom
	mysqldump -u ${username} -p${passwd} -T${queryTable} $target chr${f}_${queryTable}
end
cd $queryTable
foreach f (chr*_${queryTable}.txt)
    awk '{print($17,$18)}' $f > $f.tss
end

#create single line and dividedByBestMouse samples (look in file "sout" for [MING] and [MAXG] )
if ( -e smpJobs ) rm smpJobs
foreach f (*.L.txt)
    echo 'txtToXY '$f' 0 8' >> smpJobs
end
para make smpJobs
    
#average single line samples (5 times per level, 3 levels at most)    
if( -e avgJobs ) rm avgJobs
foreach f (*.smp1)
    echo 'averageZoomLevels 50 2500 ' $target ' ' $f ' -max' >> avgJobs
end
para make avgJobs

mkdir zoom1
mv *.zoom_1 zoom1/
mkdir zoom50
mv *.zoom_50 zoom50/
mkdir zoom2500
mv *.zoom_2500 zoom2500/

mkdir smp1
mv *.smp1 smp1/
mkdir smp
mv *.smp smp/
mkdir L
mv *.L.txt L/
mkdir atleast15
mv *.atleast15 atleast15/
  

#make samples more than one per line after averaging (ignore zoom1)

if( -e groupJobs ) rm groupJobs
foreach f (zoom1/* zoom50/* zoom2500/*)
    echo 'groupSamples 1000 '$f ' ' $f'.grp' >> groupJobs
end
para make groupJobs

cd ./zoom50
foreachChrom
    awk 'BEGIN{i=0}($4 != "Empty"){printf("%s\t%s\t%s\t%s.%d\t%s\t%s\t%s\t%s\t%s\n", $1,$2,$3,$4,i,$5,$6,$7,$8,$9 ); i=i+1}'\
        chr${f}.50b.smp1.zoom_50.grp > chr${f}.50b.smp1.zoom_50.uniq.grp
    hgLoadSample $target chr${f}_zoom50_${outTable} chr${f}.50b.smp1.zoom_50.uniq.grp
end

cd ../zoom2500
foreachChrom
    awk 'BEGIN{i=0}($4 != "Empty"){printf("%s\t%s\t%s\t%s.%d\t%s\t%s\t%s\t%s\t%s\n", $1,$2,$3,$4,i,$5,$6,$7,$8,$9 ); i=i+1}' \
        chr${f}.50b.smp1.zoom_2500.grp > chr${f}.50b.smp1.zoom_2500.uniq.grp
    hgLoadSample $target chr${f}_zoom2500_${outTable} chr${f}.50b.smp1.zoom_2500.uniq.grp
end


cd ../zoom1
foreachChrom
    awk 'BEGIN{i=0}($4 != "Empty"){printf("%s\t%s\t%s\t%s.%d\t%s\t%s\t%s\t%s\t%s\n", $1,$2,$3,$4,i,$5,$6,$7,$8,$9 ); i=i+1}' \
        chr${f}.50b.smp1.zoom_1.grp > chr${f}.50b.smp1.zoom_1.uniq.grp
    hgLoadSample $target chr${f}_zoom1_${outTable} chr${f}.50b.smp1.zoom_1.uniq.grp
end



cd ../smp
foreachChrom
    hgLoadSample $target chr${f}_${outTable} chr${f}.50b.smp
end





