# This file contains the basic template for the README.txt that will be
# found in the ENCODE downloads directories.  
# /hive/groups/encode/dcc/analysis/ftp/pipeline/hg19/<compositeName>
# The validator will use this template and our script to do mass update
# of the README will use this template.
# Items requiring relational replacement are marked with ++

This directory contains the downloadable files associated with an ENCODE
composite track. For more information about this data, including release
status, track description, methods, and a filterable file list with the
associated metadata go to:

http://genome.ucsc.edu/cgi-bin/hgFileUi?db=++db++&g=++composite++

In addition to data files, this directory may also contain some or all of the following:

    * files.txt - a tab-separated file listing each data file and its metadata.

    * md5sum.txt - a list of the md5sum output for each data file.

    * supplemental - a directory containing any additional files provided by the
    submitting laboratory.

    * releaseN directories - these are part of our development process and
    are seen when viewing data on hgdownload-test.

    * obsolete files - these are previous versions of data files which have 
    been revoked or replaced for some reason. If the data file is not listed 
    in files.txt or has a tag of objStatus= revoked or replaced, then the file 
    is obsolete.

WARNING: Please make every effort to determine that you are using
the latest version of the data. Revoked and replaced data files may be present in
this directory. The best way to determine that you are seeing the most
recent data files is by accessing the recommended 'Downloadable Files' user 
interface via the URL provided above.

WARNING: Data files on hgdownload-test have not been verified or released to the
public. Data files on hgdownlaod-test are subject to change without notice and 
should be used with discretion.

WARNING: Data files are RESTRICTED FROM USE in publication until the restriction
date noted for the given file in files.txt and in the recommended 'Downloadable Files' 
user interface, which can be accessed via the URL provided above. The full
policy is available at http://genome.ucsc.edu/ENCODE/terms.html.
