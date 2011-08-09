# This file contains the basic template for the README.txt that will be
# found in the ENCODE downloads directories.  
# /hive/groups/encode/dcc/analysis/ftp/pipeline/hg19/<compositeName>
# The validator will use this template and our script to do mass update
# of the README will use this template.
# Items requiring relational replacement are marked with ++
This directory contains the downloadable files associated with this ENCODE
composite track. Data files are RESTRICTED FROM USE in publication until the
restriction date noted in files.txt and in the recommended 'Downloadable Files' 
user interface listed below. The full data policy is available at
http://genome.ucsc.edu/ENCODE/terms.html.

For a filterable list of these files along with additional information such as
release status, restriction dates, track description, methods, and metadata,
please use the 'Downloadable Files' user interface at: 


     http://genome.ucsc.edu/cgi-bin/hgFileUi?db=++db++&g=++composite++


In addition to data files, this directory may also contain some or all of the
following:

    * files.txt - a file listing each data file and its metadata.

    * supplemental - a directory containing any additional files provided by 
      the submitting laboratory.

    * obsolete files - WARNING - Revoked and replaced data files may be present 
      in this directory. Please make every effort to determine that you are 
      using the latest version of the data. To do so access the recommended 
      'Downloadable Files' user interface via the URL provided above or check 
      files.txt.  If the data file is not listed in files.txt or has a tag of 
      objStatus=revoked or replaced, then the file is obsolete.

    * releaseN directories - WARNING - These are only present on the 
      genome-preview site. Please note that data files on genome-preview have 
      not been verified or released to the public. They are subject to change 
      without notice and should be used with caution.
