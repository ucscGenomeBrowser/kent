Metacheck tries to report on inconsistencies between all the various data stores that ENCODE uses.

Usage:
metaCheck - a program to validate that tables, files, and trackDb entries exist
usage:
   metaCheck database composite metaDb.ra trackDb.ra downloadDir
arguments:
   database            assembly database
   composite           name of track composite
   metaDb.ra           RA file with composite metaDb information
   trackDb.ra          RA file with composite trackDb information
   downloadDir         download directory for composite
options:
   -outMdb=file.ra     output cruft-free metaDb ra file
   -onlyCompTdb        only check trackDb entries that start with composite
   -release            set release state, default alpha


Longer explanation:

The checks are divided into four passes:
  - checking that metaDb objects of type "table" are tables in mySQL
  - checking that metaDb objects of type "file" are files in download dir
  - checking that metaDb objects of type "table" are found in trackDb.ra
  - checking that all tables in assembly that start with "composite" appear in metaDb
  - checking that all tables in assembly that start with "composite" and have a     field called "fileName" are links to a file that can be opened

---------------------------------------------------------------------------------------

The arguments now have defualts so teh usage is changed.
metaCheck - a program to validate that tables, files, and trackDb entries
exist
usage:
   metaCheck database composite
arguments:
   database            assembly database
   composite           name of track composite
options:
   -outMdb=file.ra     output cruft-free metaDb ra file
   -onlyCompTdb        only check trackDb entries that start with composite
   -release            set release state, default alpha
   -metaDb=            specify a path for the metaDb, by default, this looks in <database>'s metaDb.
   -trackDb=           specify a path for the trackDb, by default, this is looks in <database>'s trackDb.
   -downloadDir=       specify a path for the downloads directory
   -help               print out extended information about what metaCheck is doing

