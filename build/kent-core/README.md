This is a partial copy of the UCSC Genome Browser source code repository
https://github.com/ucscGenomeBrowser/kent/.  It includes all files that are
under an MIT license or in the public domain. All files can be packaged,
redistributed and modified as needed by other projects. 

The code should build with a simple "cd src; make utils". We test this on CentOS and OSX. 
If you run into problems, please open a ticket or contact genome@soe.ucsc.edu. 

If you want to file issues or pull-requests, please do that at the original repo, at 
https://github.com/ucscGenomeBrowser/kent/, not here. The changes will get merged into 
this repository automatically with the next release (see kent/build/kent-core/).

You can find short descriptions for most tools here:
http://hgdownload.cse.ucsc.edu/admin/exe/linux.x86_64/FOOTER.txt

Here are a few selected tools that are useful when building track and assembly hubs:

bedToBigBed - create a bigBed file for rectangle-shaped annotations (genes, enhancers, promoters, etc)
bigBedToBed - the inverse of bedToBigBed
bigBedInfo - Show information about a bigBed file. Can extract .autoSql field definitions.

wigToBigWig - create a bigWig file for signal (barchart) annotations from a .wig file
bigWigToWig - the inverse of wigToBigWig
bigWigInfo - show information about bigWig files
