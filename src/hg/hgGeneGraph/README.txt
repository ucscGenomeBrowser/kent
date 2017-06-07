The gene graph viewer has three dependencies:

- the "dot" binary from the graphviz package. It requires a rather recent graphviz version.
  The exact location can be specified with the hg.conf option graphvizPath.
- the Python >2.6 kent-like library "hgLib" from ../pylib
- the Python >2.6 library "MySQLdb". The makefile in ../pylib will add it to CGI-BIN, as a fallback

The gene graph tables are created with the ggTables command. ggTables parses input files
constructed with various gg* commands, ggGpmlToTag, ggKgmlToTab, ggMsrToTab,
ggPidToTab, ggPpiToTab, ggSpfToTab. These tables are loaded into hgFixed, the prefix of all
tables is "gg". The main table is "ggLink".

See the makeDoc and /hive/data/genomes/hg19/bed/interactions/
