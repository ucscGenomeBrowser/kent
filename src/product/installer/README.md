% Genome-Browser-on-the-Cloud Installation Instructions

# An install script for the UCSC Genome Browser

This script installs Mysql, Apache, Ghostscript, configures them and copies the UCSC Genome
Browser CGIs onto the local machine under /usr/local/apache/. It also deactivates the default
Apache htdocs/cgi folders, so it is best run on a new machine or at least a host that is not 
already used as a web server. The script can also download full or partial assembly databases,
can update the CGIs and remove temporary files (aka "trash cleaning").

The script has been tested with Ubuntu 14 LTS, Centos 6, Centos 6.7, Centos 7, Fedora 20 and OSX 10.10.

It has also been tested on virtual machines in Amazon EC2 (Centos 6 and Ubuntu
14) and Microsoft Azure (Ubuntu). If you do not want to download the full genome assembly,
you need to select the data centers called "San Francisco" (Amazon) or "West
Coast" (Microsoft) for best performance. Other data centers (e.g. East Coast) will require a local
copy of the genome assembly, which can mean 2TB-6TB of storage for the hg19 assembly. Note that this
exceeds the current maximum size of a single Amazon EBS volume.

Run this script as root, like this:

    sudo -i
    wget https://raw.githubusercontent.com/ucscGenomeBrowser/kent/master/src/product/installer/browserSetup.sh
    bash browserSetup.sh install

If you do not have wget installed, use curl instead:

    sudo -i
    curl https://raw.githubusercontent.com/ucscGenomeBrowser/kent/master/src/product/installer/browserSetup.sh
    bash browserSetup.sh install

# How does this work?

The script then downloads the CGIs and sets up the central Mysql database. All
potentially destructive steps require confirmation by the user (unless the -b = 
batch mode option is specified).

In particular, Mysql and Apache are installed and setup with the right package
manager (yum or apt-get or port). A default random password is set for the
Mysql root user and added to the ~/.my.cnf file of the Unix root account. 
If you already have setup Mysql, you would need to create to create the file
~/.my.cnf, the script will detect this and create a template file for you.
The script also does a few smaller things, like placing symlinks, detecting
mariadb, deactivating SELinux, finding the right path for your apache install
and adapting the Mysql socket config.

This will leave you with a genome browser accessible on localhost that loads its data 
through from genome-mysql.cse.ucsc.edu:3306 and hgdownload.cse.ucsc.edu:80. If
you are not on the US West Coast, it will be too slow for normal use but good 
enough to test if your setup is working.

# The different commands

To increase performance, the script accepts the command "minimal". It will download the
minimal tables required for reasonable performance from places on the US continent and 
possibly others, e.g. from Japan. Call it like this to trade space for performance
and download a few essential pieces of hg38:

    sudo bash browserSetup.sh minimal hg38

If the genome browser is still too slow you have to mirror all tables of a genome assembly.
By default rsync is used for the download.  Alternatively you can use
UDR, a UDP-based fast transfer protocol (option: -u). 

    sudo bash browserSetup.sh -u mirror hg38

A successful run of "mirror" will also cut the connection to UCSC: no tables
or files are downloaded anymore from the UCSC servers on-the-fly. To change
the remote on-the-fly loading, specify the option -o (offline) or 
-f (on-the-fly).

In the case of hg19, the full assembly download is 7TB big. To cut this down to
up to 2TB or even less, use the -t option, e.g.

    sudo bash browserSetup.sh -t noEncode mirror hg19

When you want to update all CGIs and fully mirrored assemblies, you can call the
script with the "update" parameter like this: 

    sudo bash browserInstall.sh update

Minimal mirrors, so those that have never mirrored a full assembly, should not 
use the update command, but rather just re-run the minimal command, as it will
just update the minimal tables. You may want to add this command to your crontab,
maybe run it every day, so your local tables stay in sync with UCSC:

    sudo bash browserInstall.sh minimal hg19 hg38

To update only the CGI software parts of the browser and not the data, use the
"cgiUpdate" command. This is a problematic update, functions may break if the
data needed for them is not available. In most circumstances, we recommend you
use the "mirror" or "update" commands instead.

    sudo bash browserInstall.sh cgiUpdate

You probably also want to add a cleaning command to your crontab to remove 
the temporary files that are created during genome browser usage. They accumulate
in /usr/local/apache/trash and can quickly take up a lot of space. A command like
this could be added to your crontab file:

    sudo bash browserInstall.sh clean

If you find a bug or your linux distribution is not supported, please file pull
requests, open a github issue or contact genome-mirror@soe.ucsc.edu. 

More details about the Genome Browser installation are at
http://genome-source.cse.ucsc.edu/gitweb/?p=kent.git;a=tree;f=src/product

# All options

Here is the full listing of commands and options supported by browserSetup.sh: 

```
./browserSetup.sh [options] [command] [assemblyList] - UCSC genome browser install script

command is one of:
  install    - install the genome browser on this machine
  minimal    - download only a minimal set of tables. Missing tables are
               downloaded on-the-fly from UCSC.
  mirror     - download a full assembly (also see the -t option below).
               No data is downloaded on-the-fly from UCSC.
  update     - update the genome browser binaries and data, downloads
               all tables of an assembly
  cgiUpdate  - update the genome browser binaries and data, downloads
               all tables of an assembly
  clean      - remove temporary files of the genome browser, do not delete
               any custom tracks

parameters for 'install' and 'minimal':
  <assemblyList>     - download Mysql + /gbdb files for a space-separated
                       list of genomes

examples:
  bash ./browserSetup.sh install     - install Genome Browser, do not download any genome
                        assembly, switch to on-the-fly mode (see the -f option)
  bash ./browserSetup.sh minimal hg19 - download only the minimal tables for the hg19 assembly
  bash ./browserSetup.sh mirror hg19 mm9 - download hg19 and mm9, switch
                        to offline mode (see the -o option)
  bash ./browserSetup.sh mirror -t noEncode hg19  - install Genome Browser, download hg19 
                        but no ENCODE tables and switch to offline mode 
                        (see the -o option)
  bash ./browserSetup.sh update     -  update the Genome Browser CGI programs
  bash ./browserSetup.sh clean      -  remove temporary files

All options have to precede the list of genome assemblies.

options:
  -a   - use alternative download server at SDSC
  -b   - batch mode, do not prompt for key presses
  -t   - only download track selection, requires a value.
         This option is only useful for Human/Mouse assemblies.
         Download only certain tracks, possible values:
         noEncode = do not download any tables with the wgEncode prefix, 
                    except Gencode genes, saves 4TB/6TB for hg19
         bestEncode = our ENCODE recommendation, all summary tracks, saves
                    2TB/6TB for hg19
         main = only RefSeq/Gencode genes and SNPs, total 5GB for hg19
  -u   - use UDR (fast UDP) file transfers for the download.
         Requires at least one open UDP incoming port 9000-9100.
         (UDR is not available for Mac OSX)
  -o   - switch to offline-mode. Remove all statements from hg.conf that allow
         loading data on-the-fly from the UCSC download server. Requires that
         you have downloaded at least one assembly, using the '"download"' 
         command, not the '"mirror"' command.
  -f   - switch to on-the-fly mode. Change hg.conf to allow loading data
         through the internet, if it is not available locally. The default mode
         unless an assembly has been provided during install
  -h   - this help message
```

# Credits
* Daniel Vera (bio.fsu.edu) for his RHEL install notes.
* Bruce O'Neill, Malcolm Cook for feedback

