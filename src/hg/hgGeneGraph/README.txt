The gene graph viewer has three dependencies:

- the "dot" binary from the graphviz package.
  The exact location can be specified with the hg.conf option graphvizPath.
  There is a statically-compiled version in https://hgdownload.soe.ucsc.edu/admin/exe/external.x86_64/loader/
  Making a static build of graphviz is extremely time consuming, it can easily take a day to get all the 
  dependencies in the right version. Some documentation is here: http://genomewiki.ucsc.edu/index.php/Graphviz_static_build
- the Python >3.6 kent-like library "hgLib3" from ../pylib

The gene graph tables are created with the ggTables command. ggTables parses input files
constructed with various gg* commands, ggGpmlToTag, ggKgmlToTab, ggMsrToTab,
ggPidToTab, ggPpiToTab, ggSpfToTab. These tables are loaded into hgFixed, the prefix of all
tables is "gg". The main table is "ggLink".

See the makeDoc and /hive/data/genomes/hg19/bed/interactions/
