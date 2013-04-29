kentSrc = ../../../../..
A = allenCleanup
include ${kentSrc}/inc/userApp.mk

test:: allenCleanUp
	allenCleanup \
	/san/sanvol1/visiGene/offline/allenBrain/imageDisk \
	/san/sanvol1/visiGene/gbdb/full/inSitu/Mouse/allenBrain \
     	/san/sanvol1/visiGene/offline/allenBrain/imageDisk/May_06/allen20061204combined.tab \
	/san/sanvol1/visiGene/offline/allenBrain/probesAndData/allen20061204.tab \
	> log


clean::
	rm -f log
