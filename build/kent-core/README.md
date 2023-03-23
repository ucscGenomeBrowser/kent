This is a partial copy of the UCSC Genome Browser source code repository
https://github.com/ucscGenomeBrowser/kent/.  It includes all files that are
under an MIT license or in the public domain. All files can be packaged,
redistributed and modified as needed by other projects. 

The code should build with a simple "make". The binaries can then be found under "bin/".
We tested this on CentOS 8, Ubuntu 20 and OSX. 

To install the required libraries on Ubuntu 20, run the following command:

    apt install make gcc g++ libpng-dev uuid-dev libmariadbclient-dev

If you run into problems, you can contact genome@soe.ucsc.edu. 
If you want to file issues or pull-requests, please do that at the original repo, at 
https://github.com/ucscGenomeBrowser/kent/, not here. Changes to that repo will
get merged into this repository automatically with the next release (see
kent/build/kent-core/).

You can find short descriptions for most command line tools here:
http://hgdownload.cse.ucsc.edu/admin/exe/linux.x86_64/FOOTER.txt

Here are a few selected tools that are useful when building track and assembly hubs:

bedToBigBed - create a bigBed file for rectangle-shaped annotations (genes, enhancers, promoters, etc)

|Tool   |Description   |
|---|---|
|bedToBigBed | create a bigBed file for rectangle-shaped annotations (genes, enhancers, promoters, etc)   |
|bigBedToBed | the inverse of bedToBigBed |
|bigBedInfo  | Show information about a bigBed file. Can extract .autoSql field definitions |
|wigToBigWig | create a bigWig file for signal (barchart) annotations from a .wig file |
|bigWigToWig | the inverse of wigToBigWig |
|bigWigInfo | show information about bigWig files |
