% Installation of a UCSC Genome Browser on a local machine ("mirror")

# Considerations before installing a Genome Browser

Like most web servers, running a Genome Browser installation at your institution,
even for your own department, requires a Unix machine, disk space (6TB for hg19), and the
resources to update the site and underlying OS regularily.  
You may want to consider these alternatives before embarking on a full UCSC
Genome Browser installation directly on your server.

1.  If you only want to include a genome browser view into your webpage for
    already existing genomes, you can use an &lt;iframe&gt; tag and point it to 
    [http://genome.ucsc.edu/cgi-bin/hgRenderTracks](http://genome.ucsc.edu/cgi-bin/hgRenderTracks),
    which will show only the main browser graphic without any decorations.

    You can then use various parameters to adapt this graphic to your use case
    (e.g. set position, switch on/off tracks or highlight a range with a color), 
    see our [manual pages](https://genome.ucsc.edu/goldenpath/help/customTrack.html#SHARE) for a list
    of the parameters.
    
2.  [Assembly Hubs](https://genome.ucsc.edu/goldenPath/help/hgTrackHubHelp.html#Assembly): 
    Assembly Hubs allow you to prepare any FASTA file, add annotations and use
    the Genome Browser to visualize it. All you need is a webserver where you
    save the indexed genome sequence and files to annotate it, e.g. in BAM,
    bigBed or bigWig format.
    
    * Pros:
        * No installation, no updates, no long-term UNIX support necessary.
        * Stable for many years, the link to the assembly hub can be put into a manuscript.
        * For commercial users, no license is required.
    * Cons:
        * You need access to a public webserver to store the files.
        * Data is rendered at UCSC. Private data can be
        password-protected and loaded through https and restricted to UCSC's
        IP address, but has to be located on the web and accessible by the
        UCSC webserver.
        * For BLAT searches in your genome, you have to start a BLAT
        server yourself on a Unix server.
        * If your hub includes a high number of
        annotation files or HAL (multiple alignment) files and is located far
        from Santa Cruz, assembly hubs may be slower than a local mirror. This issue
        can be resolved by using a UCSC mirror closer to the assembly
        hub (e.g. use genome-euro.ucsc.edu for assembly hubs located on servers in Europe, 
        or genome-asia.ucsc.edu) or by moving your hub data to a webspace
        provider which is closer to Santa Cruz or by using a content distribution network,
        where all content is mirrored and the closest location is chosen by your provider.

3.  [Genome Browser in a Box](https://genome.ucsc.edu/goldenPath/help/gbib.html): 
    Genome Browser in a Box (GBiB) is a fully configured Virtual Machine including
    Apache and Mysql and behaves exactly like the UCSC site.
    It loads genome data from the UCSC download servers on the fly.
    Website and data updates are applied automatically every two weeks. By default, GBiB
    uses the VirtualBox virtualization software, so it can be run on any
    operating system, Windows, OSX and Linux. It does not require VirtualBox,
    the virtual machine image can easily be converted to e.g. VMWare or HyperV.
    If privacy is your main concern, you can download all genomes and
    annotation tracks you need and use your GBiB even off-line on a laptop.

    * Pros:
        * Relatively simple to install: download and double-click.
        * By default, software and data updates are managed remotely by UCSC, via an update script run every week.
        * Best performance when rendering local BAM/bigWig/bigBed files.
        * No Unix webserver needed, runs on any OS.
        * For commercial users: easier click-through licensing compared to a full multi-seat, manual license.
    * Cons:
        * Requires the free VirtualBox software or a commercial Virtualization system.
        * By default requires opening at least three outgoing ports to UCSC for MySQL, Rsync downloads 
          and BLAT in your firewall. Once all data is downloaded, no open ports are needed.
        * For maximum browsing speed, can require up to 2-6TB to store all genome annotation tracks, like a manual local installation.

If none of the above options really fulfill your needs, then you should consider setting up a full local mirror of the UCSC website.
We support mirror site installations as far as our time allows and there are many working mirror sites already all over the world.

A license is required for commercial download and/or installation of the Genome
Browser binaries and source code. No license is needed for academic, nonprofit,
and personal use. To purchase a
license, see our [License Instructions](https://genome.ucsc.edu/license/index.html)
or visit the [Genome Browser store](https://genome-store.ucsc.edu/). 

# Installing a Genome Browser locally with the GBiC installer

If you do not want to use our prepared virtual machine Genome-Browser-in-a-Box, we provide an
[installation program](https://genome.ucsc.edu/goldenPath/help/gbic.html) that sets up a fully functional mirror on all major Linux
distributions.  

It has been tested on Ubuntu 14/16,  RedHat/CentOS 6 and 7,
and Fedora 20. It is preferably used on a fresh Linux
installation, as it has to deactivate the default site config file in Apache
and fills the MySQL directory with numerous databases.  The easiest way is to
run the Genome Browser in the Cloud program in a new virtual machine. The program also works on
Docker and cloud computing virtual machines, and has been tested on the ones sold by
Amazon, Microsoft and Google.

Like GBiB, the mirror installed by the GBiC can load the annotation data either from UCSC
or from a local database copy. If you load data from UCSC and use a cloud
computing provider, it is highly advisable to run your instances in the US West
Coast / San Francisco Bay Area or San Jose data centers, as otherwise data loading can be 
very slow.

To run the installation program, please see the [GBiC user guide](https://genome.ucsc.edu/goldenPath/help/gbic.html). 

# Manual installation instructions

If the installation program does not work on your linux distribution or you prefer to
make adaptations to your mirror, we also provide [step-by-step installation
instructions](mirrorManual.html) that cover the configuration of Apache,
MySQL, the Genome Browser CGIs, temporary file removal and other topics, like
data loading through proxies.

We are also aware of these external websites that were not created by UCSC
staff and are of varying quality, but may be helpful when installing on
unusual platforms.

* [Installation on CentOS](http://bergmanlab.smith.man.ac.uk/?p=1111)
* [Installation on Ubuntu](http://enotacoes.wordpress.com/2009/09/03/installing-a-minimal-ucsc-mirror-in-ubuntu-jaunty-64-bits/)
* [Installation into a FreeBSD jail](http://www.bioinformatics.uni-muenster.de/download/ucsc), by Norbert Grundmann, Universitaet Muenster, Germany
* [Compilation of the Kent source tree on OSX with fink](http://bergmanlab.ls.manchester.ac.uk/?p=32) by Casey Bergman, Manchester, UK
* [Browser installation and new genome setup](http://www.oliverelliott.org/article/bioinformatics/tut_genomebrowser/), by Oliver Elliott, Columbia University, USA
* [Installation notes in our wiki](http://genomewiki.ucsc.edu/index.php/Browser_Installation)

# Using UDR to speed up downloads

[UDR] (UDT Enabled Rsync) is a download protocol that is very efficent
at sending large amounts of data over long distances. UDR utilizes rsync
as the transport mechanism, but sends the data over the UDT protocol.
UDR is not written or managed by UCSC. It is an open source tool created
by the [Laboratory for Advanced Computing] at the University of Chicago.
It has been tested to work under Linux, FreeBSD and Mac OSX, but may
work under other UNIX variants. The source code can be obtained through
[GitHub][UDR]. When using the Genome Browser in the Cloud installation 
program, the -u option will use UDR for all downloads.

If you are a casual or occasional manual downloader of data, there is no
need to change your method; continue to visit our download server to
download the files you need. This new protocol has been put in place to
enable huge amounts of data to be downloaded quickly over long
distances.

Typical TCP-based protocols like http, ftp and rsync have a problem in
that the further away the download source is from you, the slower the
speed becomes. Protocols like UDT/UDR allow for many UDP packets to be
sent in batch, thus allowing for much higher transmit speeds over long
distances. UDR will be especially useful for users who are downloading
from places that are far away from California. The US East Coat and the
international community will likely see much higher download speeds by
using UDR rather than rsync, http or ftp.

If you need help building the UDR binaries or have questions about how
UDR functions, please read the documentation on the GitHub page and if
necessary, contact the UDR authors via the GitHub page. We recommend
reading the documentation on the UDR GitHub page to better understand
how UDR works. UDR is written in C++. It is Open Source and is released
under the Apache 2.0 License. In order for it to work, you must have
rsync installed on your system.

For your convenience, we are offering a binary distribution of UDR for
Red Hat Enterprise Linux 6.x (or variants such as CentOS 6 or Scientific
Linux 6). You'll find both a 64-bit and 32-bit rpm [here].

Once you have a working UDR binary, either by building from source or by
installing the rpm, you can download files from either of our our
download servers in a very similar fashion to rsync. For example, using
rsync, you may want to download all of the MySQL tables for the hg19
database using either one of the following two commands:

    rsync -avP rsync://hgdownload.cse.ucsc.edu/mysql/hg19/ /my/local/hg19/
    rsync -avP hgdownload.cse.ucsc.edu::mysql/hg19/ /my/local/hg19/

Using UDR is very similar. The UDR syntax for downloading the same data
would be:

    udr rsync -avP hgdownload.cse.ucsc.edu::mysql/hg19/ /my/loc

[UDR]: https://github.com/LabAdvComp/UDR
[Laboratory for Advanced Computing]: http://www.labcomputing.org/
[here]: http://hgdownload.cse.ucsc.edu/admin/udr

# The genome-mirror mailing list

If you have questions on anything related to the UCSC Genome Browser installation, contact us at genome-mirror@soe.ucsc.edu
