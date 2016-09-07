% Genome Browser in the Cloud User Guide

# **What is Genome Browser in the Cloud?**

The Genome Browser in the Cloud (GBiC) script is a convenient script that automates the setup of a
UCSC Genome Browser mirror, including the installation and setup of MySQL (or MariaDB) 
and Apache servers.

After setting up  MySQL, Apache, and Ghostscript, the GBiC script copies the Genome
Browser CGIs onto the local machine under /usr/local/apache/. It also deactivates the default
Apache htdocs/cgi folders, so it is best run on a new machine, or at least a host that is not 
already used as a web server. The script can also download full or partial assembly databases,
update the CGIs, and remove temporary files (aka "trash cleaning").

The GBiC script has been tested with Ubuntu 14 LTS, Centos 6, Centos 6.7, 
Centos 7, and Fedora 20.

It has also been tested on virtual machines in Amazon EC2 (Centos 6 and Ubuntu 14) and Microsoft 
Azure (Ubuntu). If you want to load data on the fly from UCSC, you need to select the 
data centers "US West (N. California)" (Amazon) or "West US" (Microsoft) for best performance. 
Other data centers (e.g. East Coast) will require a local copy of the genome assembly, which 
requires 2TB-7TB of storage for the hg19 assembly. Note that this exceeds the current maximum 
size of a single Amazon EBS volume.

# **Quick Start Instructions**

Download the GBiC script from the [UCSC Genome-Browser store](https://genome-store.ucsc.edu/).

Run the script as root, like this:

    sudo -i
    bash browserSetup.sh install

The `install` command downloads and configures Apache, MySQL and Ghostscript, copies the Genome Browser
CGIs, and configures the mirror to load data remotely from UCSC. The `install` command must be
run before any other command is used.

# **How does this work?**

The GBiC script downloads the Genome Browser CGIs and sets up the central MySQL database. All
potentially destructive steps require confirmation by the user (unless the -b = 
batch mode option is specified).

In particular, MySQL and Apache are installed and setup with the right package
manager (yum or apt-get). A default random password is set for the
MySQL root user and added to the ~/.my.cnf file of the Unix root account. 
If you have already setup MySQL, you will need to create the file
~/.my.cnf. The script will detect this and create a template file for you.
The script also does a few smaller things, like placing symlinks, detecting
MariaDB, deactivating SELinux, finding the right path for your Apache install
and adapting the MySQL socket config.

This will leave you with a genome browser accessible on localhost that loads its data 
through from genome-mysql.soe.ucsc.edu:3306 and hgdownload.soe.ucsc.edu:80. If
you are not on the US West Coast, it will be too slow for normal use but good 
enough to test that your setup is working. You can then use the script to download 
assemblies of interest to your local Genome Browser, which will make it at least 
as fast as the UCSC site.

# **The commands**

The first argument of the script is called 'command' in the following. The first
command that you will need is "install", it installs the browser dependencies,
binaries and basic MySQL infrastructure:

    sudo bash browserSetup.sh install

There are a number of options supported by the GBiC script. In all cases, options must
be specified before the command. 

The following example correctly specifies the batch mode option to the script:

    sudo bash browserSetup.sh -b install

To increase performance of your Genome Browser, the script accepts the command
`minimal`. It will download the minimal tables required for reasonable
performance from places on the US continent and possibly others, e.g. from
Japan. Call it like this to trade space for performance and download a few
essential pieces of hg38:

    sudo bash browserSetup.sh minimal hg38

If the genome browser is still too slow then you will need to mirror all tables of a 
genome assembly. By default rsync is used for the download.  Alternatively you can use
UDR, a UDP-based fast transfer protocol (option: -u). 

    sudo bash browserSetup.sh -u mirror hg38

A successful run of `mirror` will also cut the connection to UCSC: no tables
or files are downloaded on-the-fly anymore from the UCSC servers. To change
the remote on-the-fly loading, specify the option -o (offline) or 
-f (on-the-fly). If you are planning on keeping sensitive data on your mirror,
you will want to disable on-the-fly loading, like so:

    sudo bash browserSetup.sh -o

In the case of hg19, the full assembly download is ~6.5TB. Limit this
to 2TB or less with the -t option: 

    sudo bash browserSetup.sh -t noEncode mirror hg19

For a full list of -t options, see the [All options](#all-options) section  or run the 
script with no arguments.

When you want to update all CGIs and fully mirrored assemblies, you can call the
script with the `update` parameter like this: 

    sudo bash browserInstall.sh update

Minimal mirrors (those that have never mirrored a full assembly) should not 
use the update command, but rather just re-run the minimal command, as it will
just update the minimal tables. You may want to add this command to your crontab,
maybe run it every day, so your local tables stay in sync with UCSC:

    sudo bash browserInstall.sh minimal hg19 hg38

To update only the browser software and not the data, use the
`cgiUpdate` command: 

    sudo bash browserInstall.sh cgiUpdate

However, software may break or not work correctly if the needed data is not available. 
Thus in most circumstances we recommend you use the `mirror` or `update` commands instead
of `cgiUpdate`.

You will also want to add a cleaning command to your crontab to remove 
the temporary files that are created during normal genome browser usage. They accumulate
in /usr/local/apache/trash and can quickly take up a lot of space. A command like
this should be added to your crontab file:

    sudo bash browserInstall.sh clean

If you find a bug or your Linux distribution is not supported, please contact 
[genome-mirror@soe.ucsc.edu](mailto:genome-mirror@soe.ucsc.edu). More details about the 
Genome Browser installation are at
<http://genome-source.soe.ucsc.edu/gitweb/?p=kent.git;a=tree;f=src/product>
 
# **All options**

Here is the full listing of commands and options supported by the GBiC script: 

```
browserSetup.sh [options] [command] [assemblyList] - UCSC genome browser install script

command is one of:
  install    - install the genome browser on this machine. This is usually 
               required before any other commands are run.
  minimal    - download only a minimal set of tables. Missing tables are
               downloaded on-the-fly from UCSC.
  mirror     - download a full assembly (also see the -t option below).
               No data is downloaded on-the-fly from UCSC.
  update     - update the genome browser software and data, updates
               all tables of an assembly, like "mirror"
  cgiUpdate  - update only the genome browser software, not the data. Not 
               recommended, see documentation.
  clean      - remove temporary files of the genome browser older than one 
               day, but do not delete any uploaded custom tracks

parameters for 'minimal', 'mirror' and 'update':
  <assemblyList>     - download MySQL + /gbdb files for a space-separated
                       list of genomes

examples:
  bash browserSetup.sh install     - install Genome Browser, do not download any genome
                        assembly, switch to on-the-fly mode (see the -f option)
  bash browserSetup.sh minimal hg19 - download only the minimal tables for the hg19 assembly
  bash browserSetup.sh mirror hg19 mm9 - download hg19 and mm9, switch
                        to offline mode (see the -o option)
  bash browserSetup.sh mirror -t noEncode hg19  - install Genome Browser, download hg19 
                        but no ENCODE tables and switch to offline mode 
                        (see the -o option)
  bash browserSetup.sh update hg19 -  update all data and all tables of the hg19 assembly
                         (in total 7TB)
  bash browserSetup.sh cgiUpdate   - update the Genome Browser CGI programs
  bash browserSetup.sh clean       -  remove temporary files older than one day

All options have to precede the command.

options:
  -a   - use alternative download server at SDSC
  -b   - batch mode, do not prompt for key presses
  -t   - only download track selection, requires a value.
         This option is only useful for Human/Mouse assemblies.
         Download only certain tracks, possible values:
         noEncode = do not download any tables with the wgEncode prefix, 
                    except Gencode genes, saves 4TB/7TB for hg19
         bestEncode = our ENCODE recommendation, all summary tracks, saves
                    2TB/7TB for hg19
         main = only RefSeq/Gencode genes and common SNPs, total 5GB for hg19
  -u   - use UDR (fast UDP) file transfers for the download.
         Requires at least one open UDP incoming port 9000-9100.
         (UDR is not available for Mac OSX)
         This option will download a udr binary to /usr/local/bin
  -o   - switch to offline-mode. Remove all statements from hg.conf that allow
         loading data on-the-fly from the UCSC download server. Requires that
         you have downloaded at least one assembly, using the '"download"' 
         command, not the '"mirror"' command.
  -f   - switch to on-the-fly mode. Change hg.conf to allow loading data
         through the internet, if it is not available locally. The default mode
         unless an assembly has been provided during install
  -h   - this help message
```

# **Credits**
* Max Haeussler for writing the script.
* Christopher Lee for testing and QA.
* Daniel Vera (bio.fsu.edu) for his RHEL install notes.
* Bruce O'Neill, Malcolm Cook for feedback.
