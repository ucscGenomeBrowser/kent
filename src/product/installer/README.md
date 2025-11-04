% Genome Browser in the Cloud User&apos;s Guide

# What is Genome Browser in the Cloud?

The Genome Browser in the Cloud (GBiC) program is a convenient tool that automates the setup of a
UCSC Genome Browser mirror. The GBiC program is for users who want to set up a full mirror of the 
UCSC Genome Browser on their server/cloud instance, rather than using 
our [dockerfile](docker.html) 
or our public website. Please see the 
[Installation of a UCSC Genome Browser on a local machine (mirror)](mirror.html#considerations-before-installing-a-genome-browser)
page for a summary of installation options, including the pros and cons of using a mirror
installation via the GBiC program vs&period; using docker.

The program works by setting up MySQL (MariaDB), Apache, and Ghostscript, and then copying the
Genome Browser CGIs onto the machine under `/usr/local/apache/`. Because it also deactivates the
default Apache htdocs/cgi folders, it is best run on a new machine, or at least a host that is not
already used as a web server. The tool can also download full or partial assembly databases,
update the Genome Browser CGIs, and remove temporary files (aka "trash cleaning").

The GBiC program has been tested and confirmed to work with Ubuntu 18/20/22/24 LTS,
Rocky 9.5, and Fedora 30/35/41.

It has also been tested on virtual machines in Amazon EC2 (Centos) and Microsoft Azure (Ubuntu).

If you want to load data on the fly from UCSC, you need to select the 
data centers "US West (N. California)" (Amazon) or "West US" (Microsoft) for best performance. 
Other data centers (e.g&period; East Coast) will require a local copy of the genome assembly, which 
requires 2TB-7TB of storage for the hg19 assembly. Note that if you mirror multiple assemblies, this
may in rare cases exceed 16TB, the current maximum size of a single Amazon EBS volume. In these
cases, you may need to use RAID striping of multiple EBS volumes, or use Amazon EFS for the /gbdb
files and Amazon Aurora for the MySQL tables. We have had one report from a user that the MySQL
performance is better when served from Aurora instead of being served from the same EC2 instance;
the cost may be higher in Aurora, depending on the type of usage.

# Quick Start Instructions

Download the GBiC program from the [UCSC Genome Browser store](https://genome-store.ucsc.edu/).

Run the program as root, like this:

    sudo bash browserSetup.sh install

The `install` command downloads and configures Apache, MySQL (MariaDB) and Ghostscript, copies the
Genome Browser CGIs, and configures the mirror to load data remotely from UCSC. The `install`
command must be run before any other command is used.

For mirror-specific help, please contact the Mirror Forum as listed on our [contact page](https://genome.ucsc.edu/contacts.html).

For an installation demonstration, see the [Genome Browser in the Cloud (GBiC) Introduction](https://www.youtube.com/watch?v=dcJERBVnjio)
video:

<p>
<iframe width="560" height="315" src="https://www.youtube.com/embed/dcJERBVnjio?rel=0"
frameborder="0" allow="accelerometer; autoplay; encrypted-media; gyroscope; picture-in-picture"
allowfullscreen></iframe></p>

# How does the GBiC program work?

The GBiC program downloads the Genome Browser CGIs and sets up the central MySQL (MariaDB) database.
All potentially destructive steps require confirmation by the user (unless the `-b` batch mode
option is specified).

In particular, MySQL (MariaDB) and Apache are installed and set up with the right package manager
(yum or apt-get). A default random password is set for the MySQL (MariaDB) root user and added to
the `~/.my.cnf` file of the Unix root account. If you have already set up MySQL (MariaDB), you must
create the `~/.my.cnf` file. The program will detect this and create a template file for you. The
program also performs some minor tasks such as placing symlinks, detecting MariaDB, deactivating
SELinux, finding the correct path for your Apache install and adapting the MySQL (MariaDB) socket
config.

This will result in a Genome Browser accessible on localhost that loads its data through
genome-mysql.soe.ucsc.edu:3306 and hgdownload.soe.ucsc.edu:80. If your geographic location is not on
the US West Coast, the performance will be too slow for normal use, though sufficient to test that
the setup is functional. A special MySQL (MariaDB) server is set up in Germany for users in Europe.
You can change the `/usr/local/apache/cgi-bin/hg.conf` genome-mysql.soe.ucsc.edu lines to
genome-euro-mysql.soe.ucsc.edu in order to get better performance. You can then use the program to
download assemblies of interest to your local Genome Browser, which will result in performance at
least as fast as the UCSC site.

### Network requirements

Your network firewall must allow outgoing connections to the following servers and ports:

* MySQL (MariaDB) connections, used to load tracks not local to your computer:
	* Port 3306 on genome-mysql.soe.ucsc.edu (128.114.119.174)
* Rsync, used to download track data:
	* TCP port 873 on hgdownload.soe.ucsc.edu (128.114.119.163)
* Download HTML descriptions on the fly:
	* TCP port 80 on hgdownload.soe.ucsc.edu (128.114.119.163)

### Root file system too small for all data
If you need to move data to another partition because the root file system is too small for all of
the assembly&apos;s data, the following steps will help complete the installation. First, do a
minimal installation with the browserSetup.sh script as described below, using just the "install"
argument. Then make symlinks to the directory that will contain the data, e.g&period; if your
biggest filesystem is called "/big":

```
sudo mv /var/lib/mysql /big/
sudo mv /gbdb /big/
sudo ln -s /big/mysql /var/lib/mysql
sudo ln -s /big/gbdb /gbdb
```

Then use the "mirror" or "minimal" arguments to browserSetup.sh to rsync over the majority of
the data.


# GBiC Commands

The first argument of the program is called `command` in the following section of this document. 
The first command that you will need is `install`, which installs the Genome Browser dependencies,
binary files and basic MySQL (MariaDB) infrastructure:

    sudo bash browserSetup.sh install

There are a number of options supported by the GBiC program. In all cases, options must
be specified before the command. 

The following example correctly specifies the batch mode option to the program:

    sudo bash browserSetup.sh -b install

To improve the performance of your Genome Browser, the program accepts the command
`minimal`. It will download the minimal tables required for reasonable
performance from places in the US and possibly others, e.g., from
Japan. Call it like this to trade space for performance and download a few
of the most used MariaDB tables for hg38:

    sudo bash browserSetup.sh minimal hg38

If the Genome Browser is still too slow, you will have to mirror all tables of a 
genome assembly. By default, rsync is used for the download. Alternatively you can use
UDR, a UDP-based fast transfer protocol (option: `-u`). 

    sudo bash browserSetup.sh -u mirror hg38

A successful run of `mirror` will also cut the connection to UCSC: no tables
or files are downloaded on-the-fly anymore from the UCSC servers. To change
the remote on-the-fly loading, specify the option `-o` (offline) or 
`-f` (on-the-fly). If you are planning to keep sensitive data on your mirror,
you will want to disable on-the-fly loading, like so:

    sudo bash browserSetup.sh -o

If you notice that BLAT no longer works for your assembly while in offline-mode, you may have to
update your GBiC mirror to restore functionality. Patch sequences are often added to assemblies
which may result in errors if your CGIs and 2bit files have not been updated to the latest version.

The full assembly download for hg19 is >7TB. Limit this
to 2TB or less with the `-t` option: 

    sudo bash browserSetup.sh -t noEncode mirror hg19

For a full list of `-t` options, see the [All GBiC options](#all-gbic-options) section or run the 
program with no arguments.

To update all CGIs and fully mirrored assemblies, call the
tool with the `update` parameter like this: 

    sudo bash browserSetup.sh update

The `update` parameter can also be used to update the data for a single assembly by providing the
UCSC assembly name (e.g. rn6, bosTau6, equCab3):

    sudo bash browserSetup.sh update bosTau9

Minimal mirror sites (those that have partially mirrored an assembly) should not 
use the `update` command, but rather just rerun the `minimal` command, so that only the minimal
tables are updated. For instance, if you have partially mirrored the hg19 and hg38 databases,
you may want to add this command to your crontab, perhaps running it every day, to keep your local 
tables in sync with those at UCSC:

    sudo bash browserSetup.sh minimal hg19 hg38

To update only the Genome Browser software and not the data, use the
`cgiUpdate` command: 

    sudo bash browserSetup.sh cgiUpdate

Software may break or not work correctly if the necessary data is not available. 
Thus in most circumstances, we recommend you use the `mirror`, `update`, or `minimal` commands
instead of `cgiUpdate`.

You will also want to add a cleaning command to your crontab to remove 
the temporary files that are created during normal Genome Browser usage. These accumulate
in `/usr/local/apache/trash` and can quickly consume significant space. A command like
this should be added to your crontab file:

    sudo bash browserSetup.sh clean

If you find that you need the Kent command line utilities in addition to the Genome Browser, the 
`addTools` command will install all the utilities into `/usr/local/bin`:

    sudo bash browserSetup.sh addTools

A majority of these utilities require an `.hg.conf` file in the users home directory. For 
an example of a minimal `.hg.conf` file, click
[here](http://genome-source.soe.ucsc.edu/gitlist/kent.git/blob/master/src/product/minimal.hg.conf).

If you find a bug, or if your Linux distribution is not supported, please contact 
[genome-mirror@soe.ucsc.edu](mailto:genome-mirror@soe.ucsc.edu). 

More details about the Genome Browser installation are available 
[here](http://genome-source.soe.ucsc.edu/gitlist/kent.git/tree/master/src/product).
 
# All GBiC options

Here is the full listing of commands and options supported by the GBiC program: 

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
  addTools   - copy the UCSC User Tools, e.g. blat, featureBits, overlapSelect,
               bedToBigBed, pslCDnaFilter, twoBitToFa, gff3ToGenePred, 
               bedSort, ... to /usr/local/bin

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

# Credits
* Max Haeussler for writing the program.
* Christopher Lee for testing and QA.
* Daniel Vera (bio.fsu.edu) for his RHEL install notes.
* Bruce O&apos;Neill, Malcolm Cook for feedback.
