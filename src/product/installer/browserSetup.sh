#!/bin/bash

# script to install/setup dependencies for the UCSC genome browser CGIs
# call it like this as root from a command line: bash browserSetup.sh

# You can easily debug this script with 'bash -x browserSetup.sh', it 
# will show all commands then

exec > >(tee -a "${HOME}/browserSetup.log") 2>&1

set -u -e -o pipefail # fail on unset vars and all errors, also in pipes

# set all locale settings to English
# searching for certain strings in command output might fail, if locale is different
export LANG=C

exitHandler() {
    if [ "$1" == "100" -o "$1" == "0" ] ; then
       exit $1 # all fine, a specific error message has already been output
    fi

    # somehow this script exited with an unknown type of error code
    echo Exit error $1 occurred on line $2
    echo The UCSC Genome Browser installation script exited with an error.
    echo Please contact us at genome-mirror@soe.ucsc.edu and send us an output log 
    echo of the command prefixed with '"bash -x"', e.g.
    echo 'bash -x browserSetup.sh install 2>&1 > install.log'
}

# only trap the exit, not the errors 
# see https://medium.com/@dirk.avery/the-bash-trap-trap-ce6083f36700
trap 'exitHandler $? $LINENO' EXIT

# ---- GLOBAL DEFAULT SETTINGS ----

# Directory where CGI-BIN and htdocs are downloaded to.
# this is the main runtime directory of the genome browser. It contains
# CGI binaries, the config file (hg.conf), temporary ("trash") files, downloaded pieces of big files
# ("udcCache") and various other runtime data needed for the browser
APACHEDIR=/usr/local/apache

# apache document root, for html documents
HTDOCDIR=$APACHEDIR/htdocs
# apache CGI-bin directory
CGIBINDIR=$APACHEDIR/cgi-bin
# directory for temporary files
TRASHDIR=$APACHEDIR/trash

# Mysql data directory for most genome annotation data
# Yes we only support mariaDB anymore, but the variables will keep their names
# Below please assume that mariadb is meant when mysql is written.
# All user progress messages mention MariaDB, and no MySQL anymore.
# (all non-mysql data is stored in /gbdb)
MYSQLDIR=/var/lib/mysql

# mysql admin binary, different path on OSX
MYSQLADMIN=mysqladmin

# mysql user account, different on OSX
MYSQLUSER=mysql

# mysql client command, will be adapted on OSX
MYSQL=mysql

# flag whether a mysql root password should be set
# the root password is left empty on OSX, as mysql
# there is not listening to a port
SET_MYSQL_ROOT="0"

# default download server, can be changed with -a
HGDOWNLOAD='hgdownload.soe.ucsc.edu'

# default GBDB dir
GBDBDIR=/gbdb

# sed -i has a different syntax on OSX
SEDINPLACE="sed -ri"

# udr binary URL
UDRURL=http://hgdownload.soe.ucsc.edu/admin/exe/linux.x86_64/udr

# rsync is a variable so it can be be set to use udr
RSYNC=rsync

# by default, everything is downloaded
RSYNCOPTS=""
# alternative?
# --include='*/' --exclude='wgEncode*.bam' hgdownload.soe.ucsc.edu::gbdb/hg19/ ./  -h

# a flagfile to indicate that the cgis were installed successfully
COMPLETEFLAG=/usr/local/apache/cgiInstallComplete.flag

# the -t option allows to download only the genome databases, not hgFixed/proteome/go/uniProt
# by default, this is off, so we download hgFixed and Co. 
ONLYGENOMES=0

# make apt-get fail on warnings
APTERR="-o APT::Update::Error-Mode=any"
# ---- END GLOBAL DEFAULT SETTINGS ----

# ---- DEFAULT CONFIG FILES ------------------
# We need to initialize hg.conf and apache.conf for the browser.
# They are inline so we do not create a dependency on another file 
# that has to be pushed to hgdownload when this script is updated

# the read command always has an error exit code for here docs, so we deactivate 
# exit on error for a moment
set +e

# syntax from http://stackoverflow.com/questions/23929235/multi-line-string-with-extra-space
read -r -d '' APACHE_CONFIG_STR << EOF_APACHECONF
# get rid of the warning at apache2 startup
ServerName genomeBrowserMirror

<VirtualHost *:*>
	ServerAdmin webmaster@localhost

	DocumentRoot $HTDOCDIR
	<Directory />
		Options +FollowSymLinks +Includes +Indexes
		XBitHack on
		AllowOverride None
		# Apache 2.2
		<IfModule !mod_authz_core.c>
		    Allow from all
		    Order allow,deny
		</IfModule>
		# Apache 2.4
		<IfModule mod_authz_core.c>
		    Require all granted
		    SSILegacyExprParser on
		</IfModule>
	</Directory>

	ScriptAlias /cgi-bin $CGIBINDIR
	<Directory "$CGIBINDIR">
		Options +ExecCGI -MultiViews +Includes +FollowSymLinks
		XBitHack on
		AllowOverride None
		# Apache 2.2
		<IfModule !mod_authz_core.c>
		    Allow from all
		    Order allow,deny
		</IfModule>
		# Apache 2.4
		<IfModule mod_authz_core.c>
		    Require all granted
		    SSILegacyExprParser on
		</IfModule>
	</Directory>

        # no indexes in the trash directory
        <Directory "$TRASHDIR">
         Options MultiViews
         AllowOverride None
	# Apache 2.2
	<IfModule !mod_authz_core.c>
	    Allow from all
	    Order allow,deny
	</IfModule>
	# Apache 2.4
	<IfModule mod_authz_core.c>
	    Require all granted
	</IfModule>
       </Directory>

</VirtualHost>
EOF_APACHECONF

read -r -d '' HG_CONF_STR << EOF_HGCONF
###########################################################
# Config file for the UCSC Human Genome server
#
# format is key=value, no spaces around the values or around the keys.
#
# For a documentation of all config options in hg.conf, see our example file at
# https://github.com/ucscGenomeBrowser/kent/blob/master/src/product/ex.hg.conf
# It includes many comments.

# from/return email address used for system emails
# NOEMAIL means that user accounts are not validated on this machine
# by sending email to users who have just signed up.
# This is set as we cannot be sure if sendmail is working from this host
# If you know that email is working, please change this 
login.mailReturnAddr=NOEMAIL

# title of host of browser, this text be shown in the user interface of
# the login/sign up screens
login.browserName=UCSC Genome Browser Mirror
# base url of browser installed
login.browserAddr=http://127.0.0.1
# signature written at the bottom of hgLogin system emails
login.mailSignature=None
# the browser login page by default uses https. This setting can be used to 
# used to make it work over http (not recommended)
login.https=off

# Credentials to access the local mysql server
db.host=localhost
db.user=readonly
db.password=access
db.socket=/var/run/mysqld/mysqld.sock
# db.port=3306

# The locations of the directory that holds file-based data
# (e.g. alignments, database images, indexed bigBed files etc)
# By default, this mirror can load missing files from the hgdownload server at UCSC
# To disable on-the-fly loading of files, comment out these lines, 
# the slow-db.* section below and the showTableCache statement.
gbdbLoc1=/gbdb/
gbdbLoc2=http://hgdownload.soe.ucsc.edu/gbdb/

# The location of the mysql server that is used if data cannot be found locally
# (e.g. chromosome annotations, alignment summaries, etc)
# To disable on-the-fly loading of mysql data, comment out these lines. 
slow-db.host=genome-mysql.soe.ucsc.edu
slow-db.user=genomep
slow-db.password=password

# if data is loaded from UCSC with slow-db, use the tableList
# mysql table to do table field name checks instead of DESCRIBE
showTableCache=tableList

# only used for debugging right now, make it obvious that this
# mirror has been installed by the installation script
isGbic=on

# direct links to Encode PDF files back to the UCSC site
# so the mirror does not need a copy of them
hgEncodeVocabDocBaseUrl=http://genome.ucsc.edu

# enable local file access for custom tracks
# By default you have to supply http:// URLs for custom track data, e.g. in bigDataUrls
# With this statement, you can allow loading from local files, as long as the path
# starts with a specific prefix
#udc.localDir=/bamFiles

# load genbank from hgFixed, this will be the default
# after v333, so not necessary anymore after July 2016
genbankDb=hgFixed

# use 2bit files instead of nib, this is only relevant in failover mode
# and for very old assemblies
forceTwoBit=yes

# if you want a different default species selection on the Gateway
# page, change this default Human to one of the genomes from the
# defaultDb table in hgcentral:
# hgsql -e "select genome from defaultDb;" hgcentral
# If you need a different version of that specific genome, change
# the defaultDb table entry, for example, a different mouse genome
# version as default:
# hgsql -e 'update defaultDb set name="mm8" where genome="Mouse"
# then this defaultGenome would read: defaultGenome=Mouse
defaultGenome=Human

# trackDb table to use. A simple value of trackDb is normally sufficient.
# In general, the value is a comma-separated list of trackDb format tables to
# search.  This supports local tracks combined with a mirror of the trackDb
# table from UCSC. The names should be in the form trackDb_suffix. This
# implies a parallel hgFindSpec format search table exists in the form
# hgFindSpec_suffix.  The specified trackDb tables are searched in the order
# specified, with the first occurance of a track being used.  You may associate
# trackDb/hgFindSpec tables with other instances of genome databases using a
# specification of dbProfile:trackDbTbl, where dbProfile is the name of a
# databases profile in hg.conf, and trackDbTbl is the name of the table in the
# remote databases.  See below for details of dbProfile
db.trackDb=trackDb
#db.trackDb=trackDb_local,trackDb

# similar to trackDb above, a mirror can also include local track groups
# This specifies the table for them
db.grp=grp

# required to use hgLogin
login.systemName=UCSC Genome Browser Mirror
# url to server hosting hgLogin
wiki.host=HTTPHOST
# Arbitrary name of cookie holding user name 
wiki.userNameCookie=gbUser
# Arbitrary name of cookie holding user id 
wiki.loggedInCookie=gbUserId

#  Use these settings to provide host, user, and password settings
customTracks.host=localhost
customTracks.user=ctdbuser
customTracks.password=ctdbpassword
customTracks.useAll=yes
customTracks.socket=/var/run/mysqld/mysqld.sock
customTracks.tmpdir=$TRASHDIR/customTrash

# central.host is the name of the host of the central MySQL
# database where stuff common to all versions of the genome
# and the user database is stored.
central.db=hgcentral
central.host=localhost
central.socket=/var/run/mysqld/mysqld.sock

# Be sure this user has UPDATE AND INSERT privs for hgcentral
central.user=readwrite
central.password=update

#	The central.domain will allow the browser cookie-cart
#	function to work.  Set it to the domain of your Apache
#	WEB server.  For example, if your browser URL is:
#	http://mylab.university.edu/cgi-bin/hgTracks?db=hg19
#	set central.domain to: mylab.university.edu
central.domain=HTTPHOST

# Change this default documentRoot if different in your installation,
# to allow some of the browser cgi binaries to find help text files
browser.documentRoot=/usr/local/apache/htdocs

#  new option for track reording functions, August 2006
hgTracks.trackReordering=on

# directory for temporary bbi file caching, default is /tmp/udcCache
# see also: README.udc
udc.cacheDir=$TRASHDIR/udcCache

# Parallel fetching of remote network resources using bigDataUrl such
# as trackHubs and customTracks
# how many threads to use (set to 0 to disable)
parallelFetch.threads=4
# how long to wait in seconds for parallel fetch to finish
parallelFetch.timeout=90

# These settings enable geographic allele frequency images on the 
# details pages for the HGDP Allele Frequency (hgdpGeo) track.
# (HGDP = Human Genome Diversity Project)
# Programs required for per-SNP geographic maps of HGDP population
# allele frequencies:
hgc.psxyPath=/usr/lib/gmt/bin/psxy
hgc.ps2rasterPath=/usr/lib/gmt/bin/ps2raster
hgc.ghostscriptPath=/usr/bin/ghostscript

# legacy setting
browser.indelOptions=on
# sql debugging: uncomment to see all SQL commands in the apache log
#JKSQL_TRACE=on
#JKSQL_PROF=on

freeType=on
freeTypeDir=../htdocs/urw-fonts

cramRef=$APACHEDIR/userdata/cramCache

EOF_HGCONF

read -r -d '' HELP_STR << EOF_HELP
$0 [options] [command] [assemblyList] - UCSC genome browser install script

command is one of:
  install    - install the genome browser on this machine. This is usually 
               required before any other commands are run.
  minimal    - download only a minimal set of tables. Missing tables are
               downloaded on-the-fly from UCSC.
  mirror     - download a full assembly (also see the -t option below).
               After completion, no data is downloaded on-the-fly from UCSC.
  offline    - put the browser offline, so no more loading of missing tables
               from UCSC on-the-fly. Much faster, but depending on how much
               you downloaded, means that you have many fewer tracks available.
  online     - put the browser online, so any missing files and tracks are
               loaded on-the-fly from UCSC.
  update     - update the genome browser software and data, updates
               all tables of an assembly, like "mirror"
  cgiUpdate  - update only the genome browser software, not the data. Not 
               recommended, see documentation.
  clean      - remove temporary files of the genome browser older than one 
               day, but do not delete any uploaded custom tracks
  addTools   - copy the UCSC User Tools, e.g. blat, featureBits, overlapSelect,
               bedToBigBed, pslCDnaFilter, twoBitToFa, gff3ToGenePred, 
               bedSort, ... to /usr/local/bin
               This has to be run after the browser has been installed, other-
               wise these packages may be missing: libpng zlib libmysqlclient
  dev        - install git/gcc/c++/freetype/etc, clone the kent repo into
               ~/kent and build the CGIs into /usr/local/apache so you can try
               them right away. Useful if you want to develop your own track 
               type. (OSX OK)
  mysql      - Patch my.cnf and recreate Mysql users. This can fix
               a broken Mysql server after an update to Mysql 8. 
               

parameters for 'minimal', 'mirror' and 'update':
  <assemblyList>     - download Mysql + /gbdb files for a space-separated
                       list of genomes

examples:
  bash $0 install     - install Genome Browser, do not download any genome
                        assembly, switch to on-the-fly mode (see the -f option)
  bash $0 minimal hg19 - download only the minimal tables for the hg19 assembly
  bash $0 mirror hg19 mm9 - download hg19 and mm9, switch
                        to offline mode (see the -o option)
  bash $0 -t noEncode mirror hg19  - install Genome Browser, download hg19 
                        but no ENCODE tables and switch to offline mode 
                        (see the -o option)
  bash $0 update hg19 -  update all data and all tables of the hg19 assembly
                         (in total 7TB). Specifying no assemblies will update all assemblies.
  bash $0 cgiUpdate   -  update the Genome Browser CGI programs
  bash $0 clean       -  remove temporary files older than one day

All options have to precede the command.

options:
  -a   - use alternative download server at Univ Bielefeld, Germany
         (used by default if faster ping time than to UCSC)
  -b   - batch mode, do not prompt for key presses
  -t   - For the "mirror" command: Track selection, requires a value.
         This option is only useful for Human/Mouse assemblies.
         Download only certain tracks, possible values:
         noEncode = do not download any tables with the wgEncode prefix, 
                    except Gencode genes, saves 4TB/6TB for hg19
         bestEncode = our ENCODE recommendation, all summary tracks, saves
                    2TB/6TB for hg19
         main = only Gencode genes and common SNPs, 5GB for hg19
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
EOF_HELP

set -e

# ----------------- END OF DEFAULT INLINE CONFIG FILES --------------------------

# ----------------- UTILITY FUNCTIONS --------------------------

# --- error handling --- 
# add some highlight so it's easier to distinguish our own echoing from the programs we call
function echo2 ()
{
    command echo '|' "$@"
}

# download file to stdout, use either wget or curl
function downloadFile ()
{
url=$1

if which wget 2> /dev/null > /dev/null; then
    wget -nv $1 -O -
else
    echo '| ' $url > /dev/stderr
    curl '-#' $1
fi
}

# download file to stdout, use either wget or curl
function downloadFileQuiet ()
{
url=$1

if which wget 2> /dev/null > /dev/null; then
    wget -q $1 -O -
else
    curl -s $1
fi
}

function errorHandler ()
{
    startMysql # in case we stopped it
    echo2 Error: the UCSC Genome Browser installation script failed with an error
    echo2 You can run it again with '"bash -x '$0'"' to see what failed.
    echo2 You can then send us an email with the error message.
    exit $?
}

# three types of data can be remote: mysql data and gbdb data 
# SHOW TABLES results can be cached remotely. All three are 
# deactivated with the following:
function goOffline ()
{
      # first make sure we do not have them commented out already
      $SEDINPLACE 's/^#slow-db\./slow-db\./g' $APACHEDIR/cgi-bin/hg.conf
      $SEDINPLACE 's/^#gbdbLoc1=/gbdbLoc1=/g' $APACHEDIR/cgi-bin/hg.conf
      $SEDINPLACE 's/^#gbdbLoc2=/gbdbLoc2=/g' $APACHEDIR/cgi-bin/hg.conf
      $SEDINPLACE 's/^#showTableCache=/showTableCache=/g' $APACHEDIR/cgi-bin/hg.conf

      # now comment them out
      $SEDINPLACE 's/^slow-db\./#slow-db\./g' $APACHEDIR/cgi-bin/hg.conf
      $SEDINPLACE 's/^gbdbLoc1=/#gbdbLoc1=/g' $APACHEDIR/cgi-bin/hg.conf
      $SEDINPLACE 's/^gbdbLoc2=/#gbdbLoc2=/g' $APACHEDIR/cgi-bin/hg.conf
      $SEDINPLACE 's/^showTableCache=/#showTableCache=/g' $APACHEDIR/cgi-bin/hg.conf
}

# activate the mysql failover and gbdb and tableList again in hg.conf
function goOnline ()
{
      # remove the comments
      $SEDINPLACE 's/^#slow-db\./slow-db\./g' $APACHEDIR/cgi-bin/hg.conf
      $SEDINPLACE 's/^#gbdbLoc1=/gbdbLoc1=/g' $APACHEDIR/cgi-bin/hg.conf
      $SEDINPLACE 's/^#gbdbLoc2=/gbdbLoc2=/g' $APACHEDIR/cgi-bin/hg.conf
      $SEDINPLACE 's/^#showTableCache=/showTableCache=/g' $APACHEDIR/cgi-bin/hg.conf
}

# wait for a key press
function waitKey ()
{
    echo2
    echo2 Press any key to continue or CTRL-C to abort.
    read -n 1
    echo2
}

# set MYCNF to the path to my.cnf
function setMYCNF ()
{
    if [ -f /etc/my.cnf.d/mariadb-server.cnf ] ; then
	# Centos 8 stream: This has to be first, because /etc/my.cnf exists on Centos 8 Stream, but it contains a mysqld section
        # by default and somehow the section doesn't seem to take effect since a specific [mariadb] section also exists in the mariadb-server.cnf.
        # As a result, we only modify the mariadb-server config file
    	MYCNF=/etc/my.cnf.d/mariadb-server.cnf 
    elif [ -f /etc/mysql/mariadb.conf.d/*-server.cnf ] ; then
	# Ubuntu with mariadb. Must come before etc/my.cnf
	MYCNF=/etc/mysql/mariadb.conf.d/*-server.cnf
    elif [ -f /etc/mysql/mysql.conf.d/mysqld.cnf ] ; then
	# Ubuntu 16, 18, 20 with mysqld
    	MYCNF=/etc/mysql/mysql.conf.d/mysqld.cnf
    elif [ -f /etc/my.cnf ] ; then
	# generic Centos 6-8
    	MYCNF=/etc/my.cnf
    elif [ -f /etc/mysql/my.cnf ] ; then
        # generic Ubuntu 14
    	MYCNF=/etc/mysql/my.cnf
    elif [ -f /usr/local/etc/my.cnf ]; then
        # brew on x86
    	MYCNF=/usr/local/etc/my.cnf
    elif [ -f /opt/homebrew/etc/my.cnf ]; then
        # homebrew on ARMs
    	MYCNF=/opt/homebrew/etc/my.cnf
    else
    	echo Could not find my.cnf. Adapt 'setMYCNF()' in browserSetup.sh and/or contact us.
    	exit 1
    fi
    echo Found Mariadb config file: $MYCNF
}

function mysqlStrictModeOff () 
{
# make sure that missing values in mysql insert statements do not trigger errors, #18368 = deactivate strict mode
# This must happen before Mariadb is started or alternative Mariadb must be restarted after this has been done
setMYCNF
echo Deactivating MySQL strict mode
sed -Ei '/^.(mysqld|server).$/a sql_mode='  $MYCNF
}

function mysqlAllowOldPasswords
# mysql >= 8 does not allow the old passwords anymore. But our client is still compiled
# with the old, non-SHA256 encryption. So we must deactivate this new feature.
# We do not support MySQL anymore, so this function could be removed
{
echo2 'Checking for Mysql version >= 8 (not officially supported by our software)'

MYSQLMAJ=`mysql -e 'SHOW VARIABLES LIKE "version";' -NB | cut -f2 | cut -d. -f1`
setMYCNF
if [ "$MYSQLMAJ" -ge 8 ] ; then
    echo2 'Mysql/MariaDB >= 8 found, checking if default-authentication allows native passwords'
    if grep -q default-authentication $MYCNF; then
        echo2 "default-authentication already set in $MYCNF"
    else
	echo2 Changing $MYCNF to allow native passwords and restarting Mysql
	echo '[mysqld]' >> $MYCNF
        echo 'default-authentication-plugin=mysql_native_password' >> $MYCNF
	stopMysql
	startMysql
    fi
fi
}


# oracle's mysql install e.g. on redhat distros does not secure mysql by default, so do this now
# this is copied from Oracle's original script, on centos /usr/bin/mysql_secure_installation
function secureMysql ()
{
        echo2
        echo2 Securing the Mysql install by removing the test user, restricting root
        echo2 logins to localhost and dropping the database named test.
        waitKey
        # do not parse .my.cnf for this, as we're sure that there is no root password yet
        # MYSQL2=`echo $MYSQL | sed -e 's/ / --no-defaults /'`
        # remove anonymous test users
        $MYSQL -e 'DELETE FROM mysql.user WHERE User="";'
        # remove remote root login
        $MYSQL -e "DELETE FROM mysql.user WHERE User='root' AND Host NOT IN ('localhost', '127.0.0.1', '::1');"
        # removing test database
        $MYSQL -e "DROP DATABASE IF EXISTS test;"
        $MYSQL -e "DELETE FROM mysql.db WHERE Db='test' OR Db='test\\_%'"
        $MYSQL -e "FLUSH PRIVILEGES;"
}

# When we install Mysql, make sure we do not have an old .my.cnf lingering around
function moveAwayMyCnf () 
{
if [ -f ~/.my.cnf ]; then
   echo2
   echo2 Mysql is going to be installed, but the file ~/.my.cnf already exists
   echo2 The file will be renamed to .my.cnf.old so it will not interfere with the
   echo2 installation.
   waitKey
   mv ~/.my.cnf ~/.my.cnf.old
fi
}

# print my various IP addresses to stdout 
function showMyAddress ()
{
echo2
if [[ "$DIST" == "OSX" ]]; then
   echo2 You can browse the genome at http://localhost
   #/usr/bin/open http://localhost
else
   echo2 You can now access this server under one of these IP addresses: 
   echo2 From same host:    http://127.0.0.1
   echo2 From same network: http://`ip route get 8.8.8.8 | awk 'NR==1 {print $NF}'`
   echo2 From the internet: http://`downloadFileQuiet http://icanhazip.com`
fi
echo2
}

# On OSX, we have to compile everything locally
# DEPRECATED. This can be used to build a native OSX app. But not supported right now.
function setupCgiOsx () 
{
    if [[ "$BUILDKENT" == "0" ]]; then
        echo2
        echo2 Downloading Precompiled OSX CGI-BIN binaries
        echo2
        cd $APACHEDIR
        downloadFile $CGIBINURL | tar xz
        chown -R $APACHEUSER:$APACHEUSER cgi-bin/
        return
    fi

    echo2
    echo2 Now downloading the UCSC source tree into $APACHEDIR/kent
    echo2 The compiled binaries will be installed into $APACHEDIR/cgi-bin
    echo2 HTML files will be copied into $APACHEDIR/htdocs
    waitKey

    export MYSQLINC=$APACHEDIR/ext/include
    export MYSQLLIBS="/$APACHEDIR/ext/lib/libmysqlclient.a -lz -lc++"
    export SSLDIR=$APACHEDIR/ext/include
    export PNGLIB=$APACHEDIR/ext/lib/libpng.a 
    # careful - PNGINCL is the only option that requires the -I prefix
    export PNGINCL=-I$APACHEDIR/ext/include
    export CGI_BIN=$APACHEDIR/cgi-bin 
    export SAMTABIXDIR=$APACHEDIR/kent/samtabix 
    export USE_SAMTABIX=1
    export SCRIPTS=$APACHEDIR/util
    export BINDIR=$APACHEDIR/util

    export PATH=$BINDIR:$PATH
    mkdir -p $APACHEDIR/util

    cd $APACHEDIR
    # get the kent src tree
    if [ ! -d kent ]; then
       downloadFile http://hgdownload.soe.ucsc.edu/admin/jksrc.zip > jksrc.zip
       unzip jksrc.zip
       rm -f jksrc.zip
    fi

    # get samtools patched for UCSC and compile it
    cd kent
    if [ ! -d samtabix ]; then
       git clone http://genome-source.soe.ucsc.edu/samtabix.git
    else
       cd samtabix
       git pull
       cd ..
    fi

    cd samtabix
    make

    # compile the genome browser CGIs
    cd $APACHEDIR/kent/src
    make libs
    make cgi-alpha
    cd hg/dbTrash
    make

    #cd hg/htdocs
    #make doInstall destDir=$APACHDIR/htdocs FIND="find ."
    # dbTrash tool needed for trash cleaning
}

# This should not be needed anymore: hgGeneGraph has moved to Python3 finally. But leaving the code in here
# anyways, as it should not hurt and some mirrors may have old Python2 code or old CGIs around. 
# We can remove this section in around 1-2 years when we are sure that no one needs Python2 anymore
function installPy2MysqlRedhat () {

    # Rocky 9
    if yum list python3-mysqlclient 2> /dev/null ; then
        yum install -y python3-mysqlclient python3 python3-devel mariadb-connector-c mariadb-common mariadb-connector-c-devel wget gcc
    else
	yum install -y python2 mysql-devel python2-devel wget gcc
	if [ -f /usr/include/mysql/my_config.h ]; then
		echo my_config.h found
	else
	    wget https://raw.githubusercontent.com/paulfitz/mysql-connector-c/master/include/my_config.h -P /usr/include/mysql/
	fi

	# this is very strange, but was necessary on Fedora https://github.com/DefectDojo/django-DefectDojo/issues/407
	# somehow the mysql.h does not have the "reconnect" field anymore in Fedora
	if grep -q "bool reconnect;" /usr/include/mysql/mysql.h ; then
	    echo /usr/include/mysql/mysql.h already has reconnect attribute
	else
	    sed '/st_mysql_options options;/a    my_bool reconnect; // added by UCSC Genome browserSetup.sh script' /usr/include/mysql/mysql.h -i.bkp
	fi

	# fedora > 34 doesn't have any pip2 package anymore so install it now
	if ! type "pip2" > /dev/null; then
	     wget https://bootstrap.pypa.io/pip/2.7/get-pip.py
	     python2 get-pip.py
	     mv /usr/bin/pip /usr/bin/pip2

	fi
	pip2 install MySQL-python
    fi
    }

# little function that compares two floating point numbers
# see https://stackoverflow.com/questions/8654051/how-can-i-compare-two-floating-point-numbers-in-bash
function numCompare () {
   awk -v n1="$1" -v n2="$2" 'BEGIN {printf (n1<n2?"<":">=") }'
}

# redhat specific part of mysql and apache installation
function installRedhat () {
    echo2 
    echo2 Installing EPEL, ghostscript, libpng
    waitKey
    # make sure we have and EPEL and ghostscript and rsync (not installed on vagrant boxes)
    # imagemagick is required for the session gallery
    yum -y update

    # Fedora doesn't have or need EPEL, however, it does not include chkconfig by default
    if cat /etc/redhat-release | egrep '(edora|ocky)' > /dev/null; then
	yum -y install chkconfig
    else
        yum -y install epel-release
    fi

    yum -y install ghostscript rsync ImageMagick R-core curl initscripts --allowerasing --nobest

    # centos 7 does not provide libpng by default
    if ldconfig -p | grep libpng12.so > /dev/null; then
        echo2 libpng12 found
    else
        yum -y install libpng12
    fi
    
    # try to activate the powertools repo. Exists on CentOS and Rocky but not Redhat
    # this may only be necessary for chkconfig? If so, we probably want to avoid using chkconfig.
    if grep 'Red Hat' /etc/redhat-release ; then
        if yum repolist | grep -i codeready ; then
            echo codeready repo enabled
        else
            echo2 This is a RHEL server and the codeready repository is not enabled. 
            echo2 Please activate it and also the EPEL reposity, then run the browserSetup command again. 
            exit 1
        fi
    else
        set +o pipefail
        echo2 Not on RHEL: Enabling the powertools repository
        yum config-manager --set-enabled powertools || true
        set -o pipefail
    fi
    
    # install apache if not installed yet
    if [ ! -f /usr/sbin/httpd ]; then
        echo2
        echo2 Installing Apache and making it start on boot
        waitKey
        yum -y install httpd chkconfig
        # start apache on boot
        chkconfig --level 2345 httpd on
        # there will be an error message that the apache 
        # mkdir -p $APACHEDIR/htdocs
        
        service httpd start
    else
        echo2 Apache already installed
    fi
    
    # create the apache config
    if [ ! -f $APACHECONF ]; then
        echo2
        echo2 Creating the Apache2 config file $APACHECONF
        waitKey
        echo "$APACHE_CONFIG_STR" > $APACHECONF
    fi
    service httpd restart

    # this triggers an error if rpmforge is not installed
    # but if rpmforge is installed, we need the option
    # psxy is not that important, we just skip it for now
    #yum -y install GMT hdf5 --disablerepo=rpmforge
    
    if [ -f /etc/init.d/iptables ]; then
       echo2 Opening port 80 for incoming connections to Apache
       waitKey
       iptables -I INPUT 1 -p tcp --dport 80 -j ACCEPT
       iptables -I INPUT 1 -p tcp --dport 443 -j ACCEPT
       service iptables save
       service iptables restart
    fi
    
    # MARIADB INSTALL ON REDHAT

    # centos7 provides only a package called mariadb-server
    # Mysql 8 does not allow copying MYISAM files anymore into the DB. 
    # -> we cannot support Mysql 8 anymore
    if yum list mariadb-server 2> /dev/null ; then
        MYSQLPKG=mariadb-server
    else
        echo2 Cannot find a mariadb-server package in the current yum repositories
        exit 100
    fi
    
    # even mariadb packages currently call their main wrapper /usr/bin/mysqld_safe
    if [ ! -f /usr/bin/mysqld_safe ]; then
        echo2 
        echo2 Installing the Mysql or MariaDB server and make it start at boot.
        waitKey

        moveAwayMyCnf
        yum -y install $MYSQLPKG
    
        # Fedora 20 names the package mysql-server but it actually contains mariadb
        MYSQLD=mysqld
        MYSQLVER=`mysql --version`
        if [[ $MYSQLVER =~ "MariaDB" ]]; then
            MYSQLD=mariadb
        fi
            
        # start mysql on boot
        yum -y install chkconfig
        chkconfig --level 2345 $MYSQLD on 

        # make sure that missing values in Mysql insert statements do not trigger errors, #18368: deactivate strict mode
        mysqlStrictModeOff

        # start mysql now
        startMysql

        secureMysql
        SET_MYSQL_ROOT=1

    else
        echo2 Mysql already installed
    fi

    # MySQL-python is required for hgGeneGraph
    # CentOS up to and including 7 default to python2, so MySQL-python is in the repos
    if yum list MySQL-python 2> /dev/null ; then
            yum -y install MySQL-python
    # Centos 8 defaults to python3 and it does not have a package MySQL-python anymore
    # So we install python2, the mysql libraries and fix up my_config.h manually
    # This is strange, but I was unable to find a different working solution. MariaDB simply does not have my_config.h
    else
	    if [ `numCompare $VERNUM 9` == "<" ] ; then
                installPy2MysqlRedhat
	    else
	        echo2 Not installing Python2, this Linux does not have it and it should not be needed anymore
	    fi
    fi

    # open port 80 in firewall
    if which firewall-cmd ; then
        echo2 Opening port HTTP/80 in firewall using the command firewall-cmd
        sudo firewall-cmd --zone=public --permanent --add-service=http
        sudo firewall-cmd --reload
    fi
}

function installOsxDevTools () 
# make sure that the xcode command line tools are installed
{
   # check for xcode
   if [ -f /usr/bin/xcode-select 2> /dev/null > /dev/null ]; then
       echo2 Found XCode
   else
       echo2
       echo2 'This installer has to compile the UCSC tools locally on OSX. We need clang. Starting installation.'
       xcode-select --install 2> /dev/null >/dev/null
   fi
}

# OSX specific setup of the installation
# NOT USED ANYMORE, but kept here for reference and for future use one day
function installOsx () 
{
   installOsxDevTools

   # on OSX by default we download a pre-compiled package that includes Apache/Mysql/OpenSSL
   # change this to 1 to rebuild all of these locally from tarballs
   BUILDEXT=${BUILDEXT:-0}
   # On OSX, by default download the CGIs as a binary package
   # change this to 1 to rebuild CGIs locally from tarball
   BUILDKENT=${BUILDKENT:-0}
   
   # URL of a tarball with a the binaries of Apache/Mysql/Openssl
   BINPKGURL=http://hgwdev.gi.ucsc.edu/~max/gbInstall/mysqlApacheOSX_10.7.tgz
   # URL of tarball with the OSX CGI binaries
   CGIBINURL=http://hgwdev.gi.ucsc.edu/~max/gbInstall/kentCgi_OSX_10.7.tgz 
   # URL of tarball with a minimal Mysql data directory
   MYSQLDBURL=http://hgwdev.gi.ucsc.edu/~max/gbInstall/mysql56Data.tgz
   # mysql/apache startup script URL, currently only for OSX
   STARTSCRIPTURL=https://raw.githubusercontent.com/maximilianh/browserInstall/master/browserStartup.sh

   # in case that it is running, try to stop Apple's personal web server, we need access to port 80
   # ignore any error messages
   #if [ -f /usr/sbin/apachectl ]; then
       #echo2 Stopping the Apple Personal Web Server
       #/usr/sbin/apachectl stop 2> /dev/null || true
       #launchctl unload -w /System/Library/LaunchDaemons/org.apache.httpd.plist 2> /dev/null || true
   #fi

   if [ -f ~/.my.cnf ]; then
       echo2
       echo2 ~/.my.cnf already exists. This will interfere with the installation.
       echo2 The file will be renamed to ~/.my.cnf.old now.
       mv ~/.my.cnf ~/.my.cnf.old
       waitKey
   fi

   # get all external software like apache, mysql , openssl
   if [ ! -f $APACHEDIR/ext/src/allBuildOk.flag ]; then
       mkdir -p $APACHEDIR/ext/src
       cd $APACHEDIR/ext/src
       if [ "$BUILDEXT" == "1" ]; then
           buildApacheMysqlOpensslLibpng
       else
           echo2
           echo2 Downloading Apache/Mysql/OpenSSL/LibPNG binary build from UCSC
           cd $APACHEDIR
           downloadFile $BINPKGURL | tar xz
           mkdir -p $APACHEDIR/ext/src/
           touch $APACHEDIR/ext/src/allBuildOk.flag
           rm -f $APACHEDIR/ext/configOk.flag 
       fi

       # directory with .pid files has to be writeable
       chmod a+w $APACHEDIR/ext/logs
       # directory with .socket has to be writeable
       chmod a+w $APACHEDIR/ext

       touch $APACHEDIR/ext/src/allBuildOk.flag

   fi

   # configure apache and mysql 
   if [ ! -f $APACHEDIR/ext/configOk.flag ]; then
       cd $APACHEDIR/ext
       echo2 Creating MariaDb config in $APACHEDIR/ext/my.cnf
       # avoid any write-protection issues
       if [ -f $APACHEDIR/ext/my.cnf ]; then
           chmod u+w $APACHEDIR/ext/my.cnf 
       fi
       echo '[mysqld]' > my.cnf
       echo "datadir = $APACHEDIR/mysqlData" >> my.cnf
       echo "default-storage-engine = myisam" >> my.cnf
       echo "default-tmp-storage-engine = myisam" >> my.cnf
       echo "skip-innodb" >> my.cnf
       echo "skip-networking" >> my.cnf
       echo "socket = $APACHEDIR/ext/mysql.socket" >> my.cnf
       echo '[client]' >> my.cnf
       echo "socket = $APACHEDIR/ext/mysql.socket" >> my.cnf

       cd $APACHEDIR
       # download minimal mysql db
       downloadFile $MYSQLDBURL | tar xz
       chown -R $MYSQLUSER:$MYSQLUSER $MYSQLDIR

       # configure apache
       echo2 Configuring Apache via files $APACHECONFDIR/httpd.conf and $APACHECONF
       echo "$APACHE_CONFIG_STR" > $APACHECONF
       # include browser config from apache config
       echo2 Appending browser config include line to $APACHECONFDIR/httpd.conf
       echo Include conf/001-browser.conf >> $APACHECONFDIR/httpd.conf
       # no need for document root, note BSD specific sed option -i
       # $SEDINPLACE 's/^DocumentRoot/#DocumentRoot/' $APACHECONFDIR/httpd.conf
       # server root provided on command line, note BSD sed -i option
       $SEDINPLACE 's/^ServerRoot/#ServerRoot/' $APACHECONFDIR/httpd.conf
       # need cgi and SSI - not needed anymore, are now compiled into apache
       $SEDINPLACE 's/^#LoadModule include_module/LoadModule include_module/' $APACHECONFDIR/httpd.conf
       $SEDINPLACE 's/^#LoadModule cgid_module/LoadModule cgid_module/' $APACHECONFDIR/httpd.conf
       # OSX has special username/group for apache
       $SEDINPLACE 's/^User .*$/User _www/' $APACHECONFDIR/httpd.conf
       $SEDINPLACE 's/^Group .*$/Group _www/' $APACHECONFDIR/httpd.conf
       # OSX is for development and OSX has a built-in apache so change port to 8080
       $SEDINPLACE 's/^Listen .*/Listen 8080/' $APACHECONFDIR/httpd.conf

       # to avoid the error message upon startup that htdocs does not exist
       # mkdir -p $APACHEDIR/htdocs
        
       # create browserStartup.sh 
       echo2 Creating $APACHEDIR/browserStartup.sh

       downloadFile $STARTSCRIPTURL > $APACHEDIR/browserStartup.sh
       chmod a+x $APACHEDIR/browserStartup.sh

       # allowing any user to write to this directory, so any user can execute browserStartup.sh
       chmod -R a+w $APACHEDIR/ext
       # only mysql does not tolerate world-writable conf files
       chmod a-w $APACHEDIR/ext/my.cnf

       # development machine + mysql only reachable from localhost = empty root pwd
       # secureMysql - not needed
       touch $APACHEDIR/ext/configOk.flag 
   fi

   echo2 Running $APACHEDIR/browserStartup.sh to start MariaDB and apache
   $APACHEDIR/browserStartup.sh
   echo2 Waiting for MariaDB to start
   sleep 5
}

# function for Debian-specific installation of mysql and apache
function installDebian ()
{
    # update repos
    if [ ! -f /tmp/browserSetup.aptGetUpdateDone ]; then
       echo2 Running apt-get update
       apt-get update $APTERR && touch /tmp/browserSetup.aptGetUpdateDone
    fi

    # the new tzdata package comes up interactive questions, suppress these
    export DEBIAN_FRONTEND=noninteractive

    echo2 Installing ghostscript and imagemagick
    waitKey
    # ghostscript for PDF export
    # imagemagick for the session gallery
    # r-base-core for the gtex tracks
    # python-mysqldb for hgGeneGraph
    apt-get $APTERR --no-install-recommends --assume-yes install ghostscript imagemagick wget rsync r-base-core curl gsfonts
    # python-mysqldb has been removed in almost all distros as of 2021
    # There is no need to install Python2 anymore. Remove the following?
    if apt-cache policy python-mysqldb | grep "Candidate: .none." > /dev/null; then 
	    echo2 The package python-mysqldb is not available anymore. Working around it
	    echo2 by installing python2 and MySQL-python with pip2
	    if apt-cache policy python2 | grep "Candidate: .none." > /dev/null; then 
               # Ubuntu >= 21 does not have python2 anymore - hgGeneGraph has been ported, so not an issue anymore
   	       echo2 Python2 package is not available either for this distro, so not installing Python2 at all.
	    else
               # workaround for Ubuntu 16-20 - keeping this section for a few years, just in case
               apt-get install $APTERR --assume-yes python2 libmysqlclient-dev python2-dev wget gcc
    	       curl https://bootstrap.pypa.io/pip/2.7/get-pip.py --output /tmp/get-pip.py
    	       python2 /tmp/get-pip.py
               if [ -f /usr/include/mysql/my_config.h ]; then
                   echo my_config.h found
               else
                   wget https://raw.githubusercontent.com/paulfitz/mysql-connector-c/master/include/my_config.h -P /usr/include/mysql/
               fi
               pip2 install MySQL-python
	    fi
    else
	    apt-get --assume-yes install python-mysqldb
    fi

    if [ ! -f $APACHECONF ]; then
        echo2
        echo2 Now installing Apache2.
        echo2 "Apache's default config /etc/apache2/sites-enable/000-default will be"
        echo2 "deactivated. A new configuration $APACHECONF will be added and activated."
        echo2 The apache modules include, cgid and authz_core will be activated.
        waitKey

        # apache and mysql are absolutely required
        # ghostscript is required for PDF output
        apt-get $APTERR --assume-yes install apache2 ghostscript
    
        # gmt is not required. install fails if /etc/apt/sources.list does not contain
        # a 'universe' repository mirror. Can be safely commented out. Only used
        # for world maps of alleles on the dbSNP page.
        # apt-get $APTERR install gmt
        
        # activate required apache2 modules
        a2enmod include # we need SSI and CGIs
        a2enmod cgid
        a2enmod authz_core # see $APACHECONF why this is necessary
        #a2dismod deflate # allows to partial page rendering in firefox during page load
        
        # download the apache config for the browser and restart apache
        if [ ! -f $APACHECONF ]; then
          echo2 Creating $APACHECONF
          echo "$APACHE_CONFIG_STR" > $APACHECONF
          a2ensite 001-browser
          a2dissite 000-default
          service apache2 restart
        fi
    fi

    if [[ ! -f /usr/sbin/mysqld ]]; then
        echo2
        echo2 Now installing the Mysql server. 
        echo2 The root password will be set to a random string and will be written
        echo2 to the file /root/.my.cnf so root does not have to provide a password on
        echo2 the command line.
        waitKey
        moveAwayMyCnf

        # do not prompt in apt-get, will set an empty mysql root password
        export DEBIAN_FRONTEND=noninteractive
	# Debian / Ubuntu 20 defaults to Mysql 8 and Mysql 8 does not allow rsync of myisam anymore
	# -> we require mariaDb now
        apt-get $APTERR --assume-yes install mariadb-server

        mysqlStrictModeOff
        startMysql
        # flag so script will set mysql root password later to a random value
        SET_MYSQL_ROOT=1
    fi

}

# download apache mysql libpng openssl into the current dir
# and build them into $APACHEDIR/ext
function buildApacheMysqlOpensslLibpng () 
{
echo2
echo2 Now building cmake, openssl, pcre, apache and mysql into $APACHEDIR/ext
echo2 This can take up to 20 minutes on slower machines
waitKey
# cmake - required by mysql
# see http://mac-dev-env.patrickbougie.com/cmake/
echo2 Building cmake
curl --remote-name http://www.cmake.org/files/v3.1/cmake-3.1.3.tar.gz
tar -xzvf cmake-3.1.3.tar.gz
cd cmake-3.1.3
./bootstrap --prefix=$APACHEDIR/ext
make -j2
make install
cd ..
rm cmake-3.1.3.tar.gz

# see http://mac-dev-env.patrickbougie.com/openssl/  - required for apache
echo2 Building openssl
curl --remote-name https://www.openssl.org/source/openssl-1.0.2a.tar.gz
tar -xzvf openssl-1.0.2a.tar.gz
cd openssl-1.0.2a
./configure darwin64-x86_64-cc --prefix=$APACHEDIR/ext
# make -j2 aborted with an error
make
make install
cd ..
rm openssl-1.0.2a.tar.gz

# see http://mac-dev-env.patrickbougie.com/mysql/ 
echo2 Building mysql5.6
curl --remote-name --location https://dev.mysql.com/get/Downloads/MySQL-5.6/mysql-5.6.23.tar.gz
tar -xzvf mysql-5.6.23.tar.gz
cd mysql-5.6.23
../../bin/cmake \
  -DCMAKE_INSTALL_PREFIX=$APACHEDIR/ext \
  -DCMAKE_CXX_FLAGS="-stdlib=libstdc++" \
  -DMYSQL_UNIX_ADDR=$APACHEDIR/ext/mysql.socket \
  -DENABLED_LOCAL_INFILE=ON \
  -DWITHOUT_INNODB_STORAGE_ENGINE=1 \
  -DWITHOUT_FEDERATED_STORAGE_ENGINE=1 \
  .
make -j2
make install
cd ..
rm mysql-5.6.23.tar.gz

# pcre - required by apache
# see http://mac-dev-env.patrickbougie.com/pcre/
echo2 building pcre
curl --remote-name ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre-8.36.tar.gz
tar -xzvf pcre-8.36.tar.gz
cd pcre-8.36
./configure --prefix=$APACHEDIR/ext --disable-shared --disable-cpp
make -j2
make install
cd ..
rm pcre-8.36.tar.gz

# apache2.4
# see http://mac-dev-env.patrickbougie.com/apache/
# and http://stackoverflow.com/questions/13587001/problems-with-compiling-apache2-on-mac-os-x-mountain-lion
# note that the -deps download of 2.4.12 does not include the APR, so we get it from the original source
curl http://apache.sunsite.ualberta.ca/httpd/httpd-2.4.12-deps.tar.gz | tar xz 
curl http://apache.sunsite.ualberta.ca/httpd/httpd-2.4.12.tar.gz | tar xz 
cd httpd-2.4.12

# build APR
cd srclib/
cd apr; ./configure --prefix=$APACHEDIR/ext; make -j2; make install; cd ..
cd apr-util; ./configure --prefix=$APACHEDIR/ext --with-apr=$APACHEDIR/ext/bin/apr-1-config; make -j2; make install; cd ..
cd ..

# now compile, compile SSL statically so there is no confusion with Apple's SSL
# also include expat, otherwise Apple's expat conflicts
./configure --prefix=$APACHEDIR/ext --enable-ssl --with-ssl=$APACHEDIR/ext --enable-ssl-staticlib-deps  --enable-mods-static=ssl --with-expat=builtin --with-pcre=$APACHEDIR/ext/bin/pcre-config --enable-pcre=static --disable-shared --with-apr=$APACHEDIR/ext/bin/apr-1-config --with-apr-util=$APACHEDIR/ext/bin/apu-1-config --enable-mods-static="ssl include cgid authn_file authn_core authz_host authz_groupfile authz_user authz_core access_compat auth_basic reqtimeout include filter mime log_config env headers setenvif version unixd status autoindex dir alias"

make -j2
make install
cd ..

# libpng - required for the genome browser
curl --remote-name --location  'http://downloads.sourceforge.net/project/libpng/libpng16/1.6.16/libpng-1.6.16.tar.gz'
tar xvfz libpng-1.6.16.tar.gz 
cd libpng-1.6.16/
./configure --prefix=$APACHEDIR/ext
make -j2
make install
cd ..
rm libpng-1.6.16.tar.gz
}

function mysqlChangeRootPwd ()
{
   # first check if an old password still exists in .my.cnf
   if [ -f ~/.my.cnf ]; then
       echo2 ~/.my.cnf already exists, you might want to remove this file
       echo2 and restart the script if an error message appears below.
       echo2
   fi

   # generate a random char string
   # OSX's tr is quite picky with unicode, so change LC_ALL temporarily
   MYSQLROOTPWD=`cat /dev/urandom | LC_ALL=C tr -dc A-Za-z0-9 | head -c8` || true
   # paranoia check
   if [[ "$MYSQLROOTPWD" == "" ]]; then
       echo2 Error: could not generate a random Mysql root password
       exit 100
   fi

   echo2
   echo2 The Mysql server was installed and therefore has an empty root password.
   echo2 Trying to set mysql root password to the randomly generated string '"'$MYSQLROOTPWD'"'

   # now set the mysql root password
   if $MYSQLADMIN -u root password $MYSQLROOTPWD; then
       # and write it to my.cnf
       if [ ! -f ~/.my.cnf ]; then
           echo2
           echo2 Writing password to ~/.my.cnf so root does not have to provide a password on the 
           echo2 command line.
           echo '[client]' >> ~/.my.cnf
           echo user=root >> ~/.my.cnf
           echo password=${MYSQLROOTPWD} >> ~/.my.cnf
           chmod 600 ~/.my.cnf
           waitKey
        else
           echo2 ~/.my.cnf already exists, not changing it.
        fi 
   else
       echo2 Could not connect to mysql to set the root password to $MYSQLROOTPWD.
       echo2 A root password must have been set by a previous installation.
       echo2 Please reset the root password to an empty password by following these
       echo2 instructions: http://dev.mysql.com/doc/refman/5.0/en/resetting-permissions.html
       echo2 Then restart the script.
       echo2 Or, if you remember the old root password, write it to a file ~/.my.cnf, 
       echo2 create three lines
       echo2 '[client]'
       echo2 user=root
       echo2 password=PASSWORD
       echo2 run chmod 600 ~/.my.cnf and restart this script.
       exit 100
   fi
}

function checkCanConnectMysql ()
{
if $MYSQL -e "SHOW DATABASES;" 2> /dev/null > /dev/null; then
    true
else
    echo2 "ERROR:"
    echo2 "Cannot connect to mysql database server, a root password has probably been setup before."
    # create a little basic .my.cnf for the current root user
    # so the mysql root password setup is easier
    if [ ! -f ~/.my.cnf ]; then
       echo '[client]' >> ~/.my.cnf
       echo user=root >> ~/.my.cnf
       echo password=YOURMYSQLPASSWORD >> ~/.my.cnf
       chmod 600 ~/.my.cnf
       echo2
       echo2 A file ${HOME}/.my.cnf was created with default values
       echo2 Edit the file ${HOME}/.my.cnf and replace YOURMYSQLPASSWORD with the mysql root password that you
       echo2 defined during the mysql installation.
    else
       echo2
       echo2 A file ${HOME}/.my.cnf already exists
       echo2 Edit the file ${HOME}/.my.cnf and make sure there is a '[client]' section
       echo2 and under it at least two lines with 'user=root' and 'password=YOURMYSQLPASSWORD'.
    fi
       
    echo2 "Then run this script again."
    echo2 'If this is a fresh docker image or blank VM, you can also remove Mariadb entirely, run "rm -rf /var/lib/mysql/*"'
    echo2 "and rm -f ${HOME}/.my.cnf and then rerun this script. It will then reinstall Mariadb and define new passwords."
    exit 100
fi
}
   
# check if a program exists in the PATH
function commandExists () {
    type "$1" &> /dev/null ;
}

# DETECT AND DEACTIVATE SELINUX: if it exists and is active
function disableSelinux () 
{
# first check if the command exists on this system
# then check if it comes back with a non-zero error code, which means it is enabled
if commandExists selinuxenabled; then
    if [ selinuxenabled ]; then
       echo2
       echo2 The Genome Browser requires that SELINUX is deactivated.
       echo2 Deactivating it now with "'setenforce 0'" and in /etc/sysconfig/selinux.
       waitKey
       # deactivate selinux until next reboot
       # On Redhat 7.5 setenforce 0 returns error code 1 if selinux is disabled
       # so we simply ignore the error code here. Possibly we should use 'setstatus | grep 'Current mode'
       # instead of the error code of selinuxenabled above. selinux
       set +e
       setenforce 0
       set -e
       # permanently deactivate after next reboot
       if [ -f /etc/sysconfig/selinux ]; then
           sed -i 's/^SELINUX=enforcing/SELINUX=disabled/g' /etc/sysconfig/selinux
       fi
       # centos 7 seems to have another config file for this
       if [ -f /etc/selinux/config ]; then
           sed -i 's/^SELINUX=enforcing/SELINUX=disabled/g' /etc/selinux/config
       fi
    fi
fi
}

# setup the mysql databases for the genome browser and grant 
# user access rights
function mysqlDbSetup ()
{
    # -------------------
    # Mysql db setup
    # -------------------

    echo2
    echo2 Creating Mysql databases customTrash, hgTemp and hgcentral
    waitKey
    $MYSQL -e 'CREATE DATABASE IF NOT EXISTS customTrash;'
    $MYSQL -e 'CREATE DATABASE IF NOT EXISTS hgcentral;'
    $MYSQL -e 'CREATE DATABASE IF NOT EXISTS hgTemp;'
    $MYSQL -e 'CREATE DATABASE IF NOT EXISTS hgFixed;' # empty db needed for gencode tracks

    updateBlatServers
    
    echo2
    echo2 "Will now grant permissions to browser database access users:"
    echo2 "User: 'browser', password: 'genome' - full database access permissions"
    echo2 "User: 'readonly', password: 'access' - read only access for CGI binaries"
    echo2 "User: 'readwrite', password: 'update' - readwrite access for hgcentral DB"
    waitKey
    
    #  Full access to all databases for the user 'browser'
    #       This would be for browser developers that need read/write access
    #       to all database tables.  

    mysqlVer=`mysql -e 'SHOW VARIABLES LIKE "version";' -NB | cut -f2 | cut -d- -f1 | cut -d. -f-2`
    if [[ $mysqlVer == "5.6" || $mysqlVer == "5.5" ]] ; then
       # centos7 uses mysql 5.5 or 5.6 which doesn't have IF EXISTS so work around that here
       $MYSQL -e 'DELETE from mysql.user where user="browser" or user="readonly" or user="readwrite" or user="ctdbuser"'
    else
       $MYSQL -e "DROP USER IF EXISTS browser@localhost"
       $MYSQL -e "DROP USER IF EXISTS readonly@localhost"
       $MYSQL -e "DROP USER IF EXISTS ctdbuser@localhost"
       $MYSQL -e "DROP USER IF EXISTS readwrite@localhost"
    fi

    $MYSQL -e "FLUSH PRIVILEGES;"

    $MYSQL -e "CREATE USER browser@localhost IDENTIFIED BY 'genome'"
    $MYSQL -e "GRANT SELECT, INSERT, UPDATE, DELETE, FILE, "\
"CREATE, DROP, ALTER, CREATE TEMPORARY TABLES on *.* TO browser@localhost"
    
    # FILE permission for this user to all databases to allow DB table loading with
    #       statements such as: "LOAD DATA INFILE file.tab"
    # For security details please read:
    #       http://dev.mysql.com/doc/refman/5.1/en/load-data.html
    #       http://dev.mysql.com/doc/refman/5.1/en/load-data-local.html
    $MYSQL -e "GRANT FILE on *.* TO browser@localhost;" 
    
    #   Read only access to genome databases for the browser CGI binaries
    $MYSQL -e "CREATE USER readonly@localhost IDENTIFIED BY 'access';"
    $MYSQL -e "GRANT SELECT, CREATE TEMPORARY TABLES on "\
"*.* TO readonly@localhost;"
    $MYSQL -e "GRANT SELECT, INSERT, CREATE TEMPORARY TABLES on hgTemp.* TO "\
"readonly@localhost;"
    
    # Readwrite access to hgcentral for browser CGI binaries to keep session state
    $MYSQL -e "CREATE USER readwrite@localhost IDENTIFIED BY 'update';"
    $MYSQL -e "GRANT SELECT, INSERT, UPDATE, "\
"DELETE, CREATE, DROP, ALTER on hgcentral.* TO readwrite@localhost; "
    
    # the custom track database needs it own user and permissions
    $MYSQL -e "CREATE USER ctdbuser@localhost IDENTIFIED BY 'ctdbpassword';"
    $MYSQL -e "GRANT SELECT,INSERT,UPDATE,DELETE,CREATE,DROP,ALTER,INDEX "\
"on customTrash.* TO ctdbuser@localhost;"
    
    # removed these now for the new hgGateway page, Apr 2016
    # by default hgGateway needs an empty hg19 database, will crash otherwise
    # $MYSQL -e 'CREATE DATABASE IF NOT EXISTS hg19'
    # mm9 needs an empty hg18 database
    $MYSQL -e 'CREATE DATABASE IF NOT EXISTS hg18'
    
    $MYSQL -e "FLUSH PRIVILEGES;"
}


# install clang, brew and configure Apple's Apache, so we can build the tree on OSX
function setupBuildOsx ()
{
   # install clang
   installOsxDevTools

   which -s brew > /dev/null
   if [[ $? != 0 ]] ; then
       echo2 Homebrew not found. Installing now.
       ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
   else
       brew update
   fi
   echo2 Installing homebrew packages libpng, openssl, mariadb, git
   brew install libpng openssl mariadb git freetype

   echo2 Allowing write access for all on Apple\'s Apache htdocs/cgi-bin directories.
   echo2 The chmod command requires sudo - please enter the admin password now:
   sudo chmod a+rw /Library/WebServer/CGI-Executables
   sudo chmod a+rw /Library/WebServer/Documents

   # create symlinks to Apple's paths so all our normal makefiles work
   if [ ! -e /usr/local/apache ]; then
      echo2 Creating /usr/local/apache to fill with symlinks later
      sudo mkdir -p /usr/local/apache
      sudo chmod a+rw /usr/local/apache
   fi

   if [ ! -e /usr/local/apache/cgi-bin ]; then
      echo2 Creating symlink /usr/local/apache/cgi-bin to /Library/WebServer/CGI-Executables
      sudo ln -s /Library/WebServer/CGI-Executables /usr/local/apache/cgi-bin
   fi
   if [ ! -e /usr/local/apache/htdocs ]; then
      echo2 Creating symlink /usr/local/apache/htdocs to /Library/WebServer/Documents
      sudo ln -s /Library/WebServer/Documents/ /usr/local/apache/htdocs 
   fi

    if [[ ! -d /usr/local/apache/trash ]]; then
        mkdir -p /usr/local/apache/trash
        sudo chmod a+rwx /usr/local/apache/trash
        # cgis use ../trash to find the trash directory, but we have a symlink here
        sudo ln -fs /usr/local/apache/trash /Library/WebServer/Documents/trash
        sudo ln -fs /usr/local/apache/trash /Library/WebServer/trash
        # cgis use ../htdocs to find the htdocs directory, but we have a symlink here
        sudo ln -fs /Library/WebServer/Documents /Library/WebServer/htdocs
    fi
    
   # switch on CGIs in Apple's Apache and restart it (OSX as a BSD does not understand \s, so need to use [[:space:]])
   sudo sed -Ei '' 's/^[[:space:]]*#(LoadModule cgid_module)/\1/; s/^[[:space:]]*#(LoadModule cgi_module)/\1/' /etc/apache2/httpd.conf
   # allow server side includes
   sudo sed -Ei '' 's/^[[:space:]]*#(LoadModule include_module)/\1/' /etc/apache2/httpd.conf
   # activate server side includes and make them depend on the X flag
   sudo sed -Ei '' 's/Options FollowSymLinks Multiviews/Options FollowSymLinks Includes Multiviews/' /etc/apache2/httpd.conf
   if grep -qv XBitHack /etc/apache2/httpd.conf ; then
     sudo sed -Ei '' '/Options FollowSymLinks Includes Multiviews/ a\
XBitHack on\
SSILegacyExprParser on
' /etc/apache2/httpd.conf
   fi
   sudo /usr/sbin/apachectl restart
}

# install gcc, make etc so we can build the tree on linux
function setupBuildLinux ()
{
   echo2 Installing required linux packages from repositories: Git, GCC, G++, Mysql-client-libs, uuid, etc
   waitKey
   if [[ "$DIST" == "debian" ]]; then
      apt-get --assume-yes $APTERR install make git gcc g++ libpng-dev libmysqlclient-dev uuid-dev libfreetype-dev libbz2-dev
   elif [[ "$DIST" == "redhat" ]]; then
      yum install -y git vim gcc gcc-c++ make libpng-devel libuuid-devel freetype-devel
   else 
      echo Error: Cannot identify linux distribution
      exit 100
   fi
}

# set this machine for browser development: install required tools, clone the tree, build it
function buildTree () 
{
   if [[ "$DIST" == "OSX" ]]; then
       setupBuildOsx
   else
       setupBuildLinux
   fi

   if [ ! -e ~/kent ]; then
      echo2 Cloning kent repo into ~/kent using git with --depth=1
      echo2 Branch is: \"beta\" = our current release, beta = testing
      waitKey
      cd ~
      git clone -b beta https://github.com/ucscGenomeBrowser/kent.git --depth=1
   fi

   echo2 Now building CGIs from ~/kent to /usr/local/apache/cgi-bin 
   echo2 Copying JS/HTML/CSS to /usr/local/apache/htdocs
   waitKey
   cd ~/kent/src
   make -j8 cgi-alpha
   make -j8 doc-alpha
}

# main function, installs the browser on Redhat/Debian and potentially even on OSX
function installBrowser () 
{
    if [ -f $COMPLETEFLAG ]; then
        echo2 error: the file $COMPLETEFLAG exists. It seems that you have installed the browser already.
        echo2 If you want to reset the Apache directory, you can run '"rm -rf /usr/local/apache"' and 
        echo2 then run this script again.
        exit 100
    fi

    echo '--------------------------------'
    echo UCSC Genome Browser installation
    echo '--------------------------------'
    echo CPU type: $MACH, detected OS: $OS/$DIST, Release: $VER, Version: $VERNUM 
    echo 
    echo This script will go through three steps:
    echo "1 - setup apache and mysql, open port 80, deactivate SELinux"
    echo "2 - copy CGI binaries into $CGIBINDIR, html files into $HTDOCDIR"
    echo "3 - optional: download genome assembly databases into mysql and /gbdb"
    echo
    echo This script will now install and configure MariaDB and Apache if they are not yet installed. 
    echo "Your distribution's package manager will be used for this."
    echo If MariaDB is not installed yet, it will be installed, secured and a root password defined.
    echo The MariaDB root password will be written into root\'s \~/.my.cnf
    echo
    echo This script will also deactivate SELinux if active and open port 80/http.
    waitKey

    # -----  OS - SPECIFIC part -----
    if [ ! -f $COMPLETEFLAG ]; then
       if [[ "$DIST" == "OSX" ]]; then
 	  echo2 OSX: build the CGIs from scratch using clang, brew and git
          buildTree
       elif [[ "$MACH" == "aarch64" ]]; then
          echo2 Linux, but ARM CPU: Need to build CGIs and htdocs locally from source using gcc, make and git
          buildTree
       elif [[ "$DIST" == "debian" ]]; then
          installDebian
       elif [[ "$DIST" == "redhat" ]]; then
          installRedhat
       fi
    fi
    # OS-specific mysql/apache installers can SET_MYSQL_ROOT to 1 to request that the root
    # mysql user password be changed

    # ---- END OS-SPECIFIC part -----

    mysqlAllowOldPasswords

    if [[ "${SET_MYSQL_ROOT}" == "1" ]]; then
       mysqlChangeRootPwd
    fi

    # Ideally, setup modern R fonts like at UCSC:
    # Rscript -e "install.packages(c('showtext', 'curl'), repos='http://cran.us.r-project.org')

    # before we do anything else with mysql
    # we need to check if we can access it. 
    # so test if we can connect to the mysql server
    checkCanConnectMysql

    disableSelinux

    checkDownloadUdr

    # CGI DOWNLOAD AND HGCENTRAL MYSQL DB SETUP

    if [ ! -f $COMPLETEFLAG ]; then
        # test if an apache file is already present
        if [ -f "$APACHEDIR" ]; then
            echo2 error: please remove the file $APACHEDIR, then restart the script with "bash $0".
            exit 100
        fi
    fi

    # check if /usr/local/apache is empty
    # on OSX, we had to create an empty htdocs, so skip this check there
    if [ -d "$APACHEDIR" -a "$OS" != "OSX" ]; then
        echo2 error: the directory $APACHEDIR already exists.
        echo2 This installer has to overwrite it, so please move it to a different name
        echo2 or remove it. Then start the installer again with "bash $0 install"
        exit 100
    fi

    mysqlDbSetup

    # setup the cram cache so remote cram files will load correctly
    mkdir -p $APACHEDIR/userdata/cramCache/{error,pending}
    chmod -R 777 $APACHEDIR/userdata/cramCache

    # -------------------
    # CGI installation
    # -------------------
    echo2
    echo2 Creating $CGIBINDIR and $HTDOCDIR and downloading contents from UCSC
    waitKey
    
    # create apache directories: HTML files, CGIs, temporary and custom track files
    mkdir -p $HTDOCDIR $CGIBINDIR $TRASHDIR $TRASHDIR/customTrash
    
    # the CGIs create links to images in /trash which need to be accessible from htdocs
    cd $HTDOCDIR
    if [ ! -e $TRASHDIR ]; then
        ln -fs $TRASHDIR
    fi

    # write the sample hg.conf to the cgi-bin directory
    echo2 Creating Genome Browser config file $CGIBINDIR/hg.conf
    echo "$HG_CONF_STR" > $CGIBINDIR/hg.conf
    
    # hg.conf tweaks
    # redhat distros have the same default socket location set in mysql as
    # in our binaries. To allow mysql to connect, we have to remove the socket path.
    # Also change the psxy path to the correct path for redhat, /usr/bin/
    if [ "$DIST" == "redhat" ]; then
       echo2 Adapting mysql socket locations in $APACHEDIR/cgi-bin/hg.conf
       sed -i "/socket=/s/^/#/" $CGIBINDIR/hg.conf
       sed -i "/^hgc\./s/.usr.lib.gmt.bin/\/usr\/bin/" $CGIBINDIR/hg.conf
    elif [ "$DIST" == "OSX" ]; then
       # in OSX adapt the sockets
       # note that the sed -i syntax is different from linux
       echo2 Adapting mysql socket locations in $CGIBINDIR/hg.conf
       sockFile=`mysql -NBe 'show variables like "socket"' | cut -f2`
       $SEDINPLACE "s|^#?socket=.*|socket=$sockFile|" $CGIBINDIR/hg.conf
       $SEDINPLACE "s|^#?customTracks.socket.*|customTracks.socket=$sockFile|" $CGIBINDIR/hg.conf
       $SEDINPLACE "s|^#?db.socket.*|db.socket=$sockFile|" $CGIBINDIR/hg.conf
       $SEDINPLACE "s|^#?central.socket.*|central.socket=$sockFile|" $CGIBINDIR/hg.conf
    fi
    # check if UCSC or genome-euro MySQL server is closer
    echo comparing latency: genome.ucsc.edu Vs. genome-euro.ucsc.edu
    eurospeed=$( (time -p (for i in `seq 10`; do curl -sSI genome-euro.ucsc.edu > /dev/null; done )) 2>&1 | grep real | cut -d' ' -f2 )
    ucscspeed=$( (time -p (for i in `seq 10`; do curl -sSI genome.ucsc.edu > /dev/null; done )) 2>&1 | grep real | cut -d' ' -f2 )
    if [[ $(awk '{if ($1 <= $2) print 1;}' <<< "$eurospeed $ucscspeed") -eq 1 ]]; then
       echo genome-euro seems to be closer
       echo modifying hg.conf to pull data from genome-euro instead of genome
       $SEDINPLACE s/slow-db.host=genome-mysql.soe.ucsc.edu/slow-db.host=genome-euro-mysql.soe.ucsc.edu/ $CGIBINDIR/hg.conf
       $SEDINPLACE "s#gbdbLoc2=http://hgdownload.soe.ucsc.edu/gbdb/#gbdbLoc2=http://hgdownload-euro.soe.ucsc.edu/gbdb/#" $CGIBINDIR/hg.conf
       HGDOWNLOAD=hgdownload-euro.soe.ucsc.edu
    else
       echo genome.ucsc.edu seems to be closer
       echo not modifying $CGIBINDIR/hg.conf
    fi

    # download the CGIs
    if [[ "$OS" == "OSX" ]]; then
        #setupCgiOsx
        echo2 Running on OSX, assuming that CGIs are already built into /usr/local/apache/cgi-bin
    elif [[ "$MACH" == "aarch64" ]]; then
        echo2 Running on an ARM CPU, assuming that CGIs are already built into /usr/local/apache/cgi-bin
    else
        # don't download RNAplot, it's a 32bit binary that won't work anywhere anymore but at UCSC
        # this means that hgGene cannot show RNA structures but that's not a big issue
        $RSYNC -avzP --exclude=RNAplot $HGDOWNLOAD::cgi-bin/ $CGIBINDIR/
        # now add the binaries for dot and RNAplot 
        $RSYNC -avzP $HGDOWNLOAD::genome/admin/exe/external.x86_64/RNAplot $CGIBINDIR/
        $RSYNC -avzP $HGDOWNLOAD::genome/admin/exe/external.x86_64/loader/dot_static $CGIBINDIR/loader/
    fi

    # download the html docs, exclude some big files on OSX
    # cannot do this when we built the tree outselves, as the .js versions will not match the C code
    if [ "$OS" == "OSX" -o "$MACH" == "aarch64" ]; then
            # but we need the new font files
            $RSYNC -azP hgdownload.soe.ucsc.edu::htdocs/urw-fonts/ /usr/local/apache/htdocs/urw-fonts/
            echo2 Not syncing most of htdocs folder, assuming that these were built from source.
            echo2 PDF and other large files only present at UCSC will be missing from htdocs.
            waitKey
    else
            rm -rf $APACHEDIR/htdocs/goldenpath
            $RSYNC -avzP --exclude ENCODE/**.pdf $HGDOWNLOAD::htdocs/ $HTDOCDIR/
    fi
    
    # assign all files just downloaded to a valid user. 
    # This also allows apache to write into the trash dir
    if [ "$OS" == "OSX" ]; then
        echo2 OSX: Not chowning /usr/local/apache subdirectories, as not running as root
    else
        chown -R $APACHEUSER:$APACHEUSER $CGIBINDIR $HTDOCDIR $TRASHDIR
    fi
    
    touch $COMPLETEFLAG

    echo2 Install complete. You should now be able to point your web browser to this machine
    echo2 and test your UCSC Genome Browser mirror. It will be too slow for practical use.
    echo2
    echo2 Notice that this mirror is still configured to use Mysql and data files loaded
    echo2 through the internet from UCSC. From most locations on the world, this is very slow.
    echo2 It also requires an open outgoing TCP port 3306 for Mysql to genome-mysql.soe.ucsc.edu/genome-euro-mysql.soe.ucsc.edu,
    echo2 and open TCP port 80 to hgdownload.soe.ucsc.edu/hgdownload-euro.soe.ucsc.edu.
    echo2
    echo2 To finish the installation, you need to download genome data to the local
    echo2 disk. To download a genome assembly and all its files now, call this script again with
    echo2 the parameters 'download "<assemblyName1> <assemblyName2> ..."', e.g. '"'bash $0 mirror mm10 hg19'"'
    echo2 
    showMyAddress
    exit 0
}

# mkdir /gbdb or do the weird things one has to do on OSX to make this directory
function mkdirGbdb 
{
    if [[ "$OS" != "OSX" ]]; then 
       # On Linux, create /gbdb and let the apache user write to it
       mkdir -p $GBDBDIR
       # Why? hgConvert will download missing liftOver files on the fly and needs write
       # write access
       chown $APACHEUSER:$APACHEUSER $GBDBDIR
       return
    fi

    sudo mkdir -p /usr/local/gbdb
    sudo chmod a+rwx /usr/local/gbdb

    # see https://apple.stackexchange.com/questions/388236/unable-to-create-folder-in-root-of-macintosh-hd
    if [[ ! -f /etc/synthetic.conf ]] || grep -vq gbdb /etc/synthetic.conf; then
         sudo /bin/sh -c 'echo "gbdb\tusr/local/gbdb" >> /etc/synthetic.conf'
    fi
    chmod 644  /etc/synthetic.conf
    /System/Library/Filesystems/apfs.fs/Contents/Resources/apfs.util -t || true
    if ! test -L /gbdb; then
     echo "warning: apfs.util failed to create /gbdb"
    fi
    echo 'This directory is /usr/local/gbdb, see /etc/synthetic.conf' >> /gbdb/README.txt
}

# GENOME DOWNLOAD: mysql and /gbdb
function downloadGenomes
{
    DBS=$*
    GENBANKTBLS=""
    if [ "$DBS" == "" ] ; then
        echo2 Argument error: the '"download"' command requires at least one assembly name, like hg19 or mm10.
        exit 100
    fi

    echo2
    echo2 Downloading databases $DBS plus hgFixed/proteome/go from the UCSC download server
    echo2
    echo2 Determining download file size... please wait...

    if [ "$ONLYGENOMES" == "0" ]; then
        if [[ "$DBS" =~ hg|mm|rn3|rn4|sacCer|dm3|danRer3|ce6 ]]; then
            echo2 Downloading $DBS plus hgFixed proteome go hgFixed
            MYSQLDBS="$DBS proteome uniProt go hgFixed"
        else
            echo2 Downloading $DBS plus GenBank and RefSeq tables
            MYSQLDBS="$DBS"
            GENBANKTBLS="author cell description development gbCdnaInfo gbExtFile gbLoaded \
                         gbMiscDiff gbSeq gbWarn geneName imageClone keyword library \
                         mrnaClone organism productName refLink refSeqStatus \
                         refSeqSummary sex source tissue"
        fi
    else
        MYSQLDBS="$DBS"
    fi

    # rsync is doing globbing itself, so switch it off temporarily
    set -f

    # On OSX the MariaDB datadir is not under /var. On Linux distros, the MariaDB directory may have been moved.
    if [ ! -d $MYSQLDIR ]; then 
        MYSQLDIR=`mysql -NBe 'SHOW Variables WHERE Variable_Name="datadir"' | cut -f2`
    fi

    # use rsync to get total size of files in directories and sum the numbers up with awk
    for db in $MYSQLDBS; do
        rsync -avn $HGDOWNLOAD::mysql/$db/ $MYSQLDIR/$db/ $RSYNCOPTS | grep ^'total size' | cut -d' ' -f4 | tr -d ', ' 
    done | awk '{ sum += $1 } END { print "| Required space in '$MYSQLDIR':", sum/1000000000, "GB" }'
    
    if [ ! -z "$GENBANKTBLS" ]; then
        for tbl in $GENBANKTBLS; do
            rsync -avn $HGDOWNLOAD::mysql/hgFixed/${tbl}.* $MYSQLDIR/hgFixed/ $RSYNCOPTS | grep ^'total size' | cut -d' ' -f4 | tr -d ', '
        done | awk '{ sum += $1 } END { print "| Required space in '$MYSQLDIR'/hgFixed:", sum/1000000000, "GB" }'
    fi

    for db in $DBS; do
        rsync -avn $HGDOWNLOAD::gbdb/$db/ $GBDBDIR/$db/ $RSYNCOPTS | grep ^'total size' | cut -d' ' -f4 | tr -d ','
    done | awk '{ sum += $1 } END { print "| Required space in '$GBDBDIR':", sum/1000000000, "GB" }'

    echo2
    echo2 Currently available disk space on this system:
    echo2
    df -h  | awk '{print "| "$0}'
    echo2 
    echo2 If your current disk space is not sufficient, you can press ctrl-c now and 
    echo2 use a 'network storage server volume (e.g. NFS) or add cloud provider storage'
    echo2 '(e.g. Openstack Cinder Volumes, Amazon EBS, Azure Storage)'
    echo2 Please refer to the documentation of your cloud provider or storage
    echo2 system on how to add more storage to this machine.
    echo2
    echo2 When you are done with the mount:
    echo2 Move the contents of $GBDBDIR and $MYSQLDIR onto these volumes and
    echo2 symlink $GBDBDIR and $MYSQLDIR to the new locations. You might have to stop 
    echo2 Mysql temporarily to do this.
    echo2 Example commands:
    echo2 "    mv /gbdb /bigData/gbdb && ln -s /bigData/gbdb /gbdb"
    echo2 "    mv /var/lib/mysql /bigData/mysql && ln -s /bigData/mysql /var/lib/mysql"
    echo2
    echo2 You can interrupt this script with CTRL-C now, add more space and rerun the 
    echo2 script later with the same parameters to start the download.
    echo2
    waitKey

    # now do the actual download of mysql files
    for db in $MYSQLDBS; do
       echo2 Downloading Mysql files for mysql database $db
       $RSYNC --progress -avp $RSYNCOPTS $HGDOWNLOAD::mysql/$db/ $MYSQLDIR/$db/ 
    done

    if [ ! -z "$GENBANKTBLS" ]; then
        echo2 Downloading hgFixed tables
        for tbl in $GENBANKTBLS; do
            $RSYNC --progress -avp $RSYNCOPTS $HGDOWNLOAD::mysql/hgFixed/${tbl}.* $MYSQLDIR/hgFixed/
        done
    fi

    echo2 Downloading hgFixed.refLink, required for all RefSeq tracks
    $RSYNC --progress -avp $RSYNCOPTS $HGDOWNLOAD::mysql/hgFixed/refLink.* $MYSQLDIR/hgFixed/ 

    mkdirGbdb

    # download /gbdb files
    for db in $DBS; do
       echo2 Downloading $GBDBDIR files for assembly $db
       $RSYNC --progress -avp $RSYNCOPTS $HGDOWNLOAD::gbdb/$db/ $GBDBDIR/$db/
    done

    set +f

    # Alexander Stuy reported that at FSU they had a few mysql databases with incorrect users on them
    if [[ "$OS" != "OSX" ]]; then 
        chown -R $MYSQLUSER:$MYSQLUSER $MYSQLDIR/
        chown -R $APACHEUSER:$APACHEUSER $GBDBDIR/$db
    fi
    
    startMysql

    mysqlCheck

    hideSomeTracks

    goOffline # modify hg.conf and remove all statements that use the UCSC download server

    echo2
    echo2 Install complete. You should now be able to point your web browser to this machine
    echo2 and use your UCSC Genome Browser mirror.
    echo2

    showMyAddress

    echo2 If you have not downloaded the human hg38 assembly and you get an error message 
    echo2 'Could not connect to database' on the genome selection page, then modify 
    echo2 the hg.conf file and change the organism, e.g. to Mouse if you downloaded mouse.
    echo2 with a command like "'nano /usr/local/apache/cgi-bin/hg.conf'"
    echo2 Search for Human in this file and replace it with the organism of the 
    echo2 you downloaded.
    echo2 If the assembly is not the default for this organism, you also have to change 
    echo2 the mysql table hgcentral.defaultDb to the correct database for your organism, 
    echo2 e.g. '"mm9"' for Mouse, with a command like
    echo2 mysql hgcentral -e \''update defaultDb set name="mm9" where genome="Mouse"'\'
    echo2 
    echo2 Note that the installation assumes that emails cannot be sent from
    echo2 this machine. New Genome Browser user accounts will not receive confirmation emails.
    echo2 To change this, edit the file $APACHEDIR/cgi-bin/hg.conf and modify the settings
    echo2 'that start with "login.", mainly "login.mailReturnAddr"'.
    echo2
    echo2 Feel free to send any questions to our mailing list genome-mirror@soe.ucsc.edu .
    waitKey
}

# stop the mysql database server, so we can write into its data directory
function stopMysql
{
    if [ -f /etc/init.d/mysql ]; then 
            service mysql stop
    elif [ -f /etc/init.d/mysqld ]; then 
            service mysqld stop
    elif [ -f /etc/init.d/mariadb ]; then 
            service mariadb stop
    elif [ -f /usr/lib/systemd/system/mariadb.service ]; then
            # RHEL 7, etc use systemd instead of SysV
            systemctl stop mariadb
    elif which brew > /dev/null ; then
            # homebrew on ARMs or X86
    	    brew services stop mariadb  
    elif [ -f /usr/lib/systemd/system/mysql.service ]; then
            # at least seen in Fedora 17
            systemctl stop mysql
    else
        echo2 Could not find mysql nor mysqld file in /etc/init.d nor a systemd command. Please email genome-mirror@soe.ucsc.edu.
    fi
}

# start the mysql database server
function startMysql
{
    if [ -f /etc/init.d/mysql ]; then 
            service mysql start
    elif [ -f /etc/init.d/mysqld ]; then 
            service mysqld start
    elif [ -f /etc/init.d/mariadb ]; then 
            service mariadb start
    elif [ -f /usr/lib/systemd/system/mariadb.service ]; then
            # RHEL 7, etc use systemd instead of SysV
            systemctl start mariadb
    elif which brew > /dev/null ; then
            echo2 Starting Mariadb using brew
            brew services start mariadb
    elif [ -f /usr/lib/systemd/system/mysql.service ]; then
            # at least seen in Fedora 17
            systemctl start mysql
    else
        echo2 Could not find mysql nor mysqld file in /etc/init.d nor a systemd command. Please email genome-mirror@soe.ucsc.edu.
    fi
}

function mysqlCheck
# check all mysql tables. Rarely, some of them are in an unclosed state on the download server, this command will close them
{
    echo2 Checking all mysql tables after the download to make sure that they are closed
    mysqlcheck --all-databases --auto-repair --quick --fast --silent
}

function hideSomeTracks
# hide the big tracks and the ones that we are not allowed to distribute
{
    # these tables are not used for searches by default. Searches are very slow. We focus on genes.
    notSearchTables='wgEncodeGencodeBasicV19 wgEncodeGencodeCompV17 wgEncodeGencodeBasicV14 wgEncodeGencodeBasicV17 wgEncode GencodeCompV14 mgcFullMrna wgEncodeGencodeBasicV7 orfeomeMrna wgEncodeGencodePseudoGeneV14 wgEncodeGencodePseudoGeneV17 wgEncodeGencodePseudoGeneV19 wgEncodeGencodeCompV7 knownGeneOld6 geneReviews transMapAlnSplicedEst gbCdnaInfo oreganno vegaPseudoGene transMapAlnMRna ucscGenePfam qPcrPrimers transMapAlnUcscGenes transMapAlnRefSeq genscan bacEndPairs fosEndPairs'

    # these tracks are hidden by default
    hideTracks='intronEst cons100way cons46way ucscRetroAli5 mrna omimGene2 omimAvSnp'

    echo2 Hiding some tracks by default and removing some tracks from searches
    for db in $DBS; do
       echo $db
       if [ "$db" == "go" -o "$db" == "uniProt" -o "$db" == "visiGene" -o "$db" == "hgFixed" -o "$db" == "proteome" ] ; then
               continue
       fi
       for track in $hideTracks; do
            mysql $db -e 'UPDATE trackDb set visibility=0 WHERE tableName="'$track'"'
        done

       for track in $notSearchTables; do
            mysql $db -e 'DELETE from hgFindSpec WHERE searchTable="'$track'"'
        done
    done

    # cannot activate this part, not clear what to do when mirror goes offline or online
    # now fix up trackDb, remove rows of tracks that this mirror does not have locally
    #for db in `mysql -NB -e 'show databases' | egrep  'proteome|uniProt|visiGene|go$|hgFixed'`; do 
            #fixTrackDb $db trackDb; 
    #done
}

# only download a set of minimal mysql tables, to make a genome browser that is using the mysql failover mechanism
# faster. This should be fast enough in the US West Coast area and maybe even on the East Coast.
function downloadMinimal
{
    DBS=$*
    if [ "$DBS" == "" ] ; then
        echo2 Argument error: the '"minimal"' command requires at least one assembly name, like hg19 or mm10.
        exit 100
    fi

    echo2
    echo2 Downloading minimal tables for databases $DBS 

    # only these db tables are copied over by default
    minRsyncOpt="--include=cytoBand.* --include=chromInfo.* --include=cytoBandIdeo.* --include=kgColor.* --include=knownAttrs.* --include=knownGene.* --include=knownToTag.* --include=kgXref.* --include=ensemblLift.* --include=ucscToEnsembl.* --include=wgEncodeRegTfbsCells.* --include=encRegTfbsClusteredSources.* --include=tableList.* --include=refSeqStatus.* --include=wgEncodeRegTfbsCellsV3.* --include=extFile.* --include=trackDb.* --include=grp.* --include=ucscRetroInfo5.* --include=refLink.* --include=ucscRetroSeq5.* --include=ensemblLift.* --include=knownCanonical.* --include=gbExtFile.* --include=flyBase2004Xref --include=hgFindSpec.* --include=ncbiRefSeq*"

    stopMysql

    for db in $DBS; do
       echo2 Downloading Mysql files for mysql database $db
       $RSYNC $minRsyncOpt --exclude=* --progress -avp $RSYNCOPTS $HGDOWNLOAD::mysql/$db/ $MYSQLDIR/$db/ 
       chown -R $MYSQLUSER:$MYSQLUSER $MYSQLDIR/$db
    done

    echo2 Copying hgFixed.trackVersion, required for most tracks
    $RSYNC --progress -avp $RSYNCOPTS $HGDOWNLOAD::mysql/hgFixed/trackVersion.* $MYSQLDIR/hgFixed/ 
    echo2 Copying hgFixed.refLink, required for RefSeq tracks across all species
    $RSYNC --progress -avp $RSYNCOPTS $HGDOWNLOAD::mysql/hgFixed/refLink.* $MYSQLDIR/hgFixed/
    chown -R $MYSQLUSER:$MYSQLUSER $MYSQLDIR/hgFixed

    startMysql

    mysqlCheck

    hideSomeTracks

    echo2 
    echo2 The mirror should be functional now. It contains some basic assembly tables 
    echo2 and will download missing data from the UCSC servers. This requires
    echo2 two open ports, outgoing, TCP, from this machine:
    echo2 - to genome-mysql.soe.ucsc.edu, port 3306, to load MySQL tables
    echo2 - to hgdownload.soe.ucsc.edu, port 80, to download non-MySQL data files
    echo2 - or the above two servers European counterparts:
    echo2   genome-euro-mysql.soe.ucsc.edu and hgdownload-euro.soe.ucsc.edu
    echo2
    showMyAddress
    goOnline
}

function checkDownloadUdr () 
# download the faster downloader udr that is compatible with rsync and change the global variable RSYNC
{
    if [[ "$OS" == "OSX" ]]; then 
        return
    fi
    if [[ "$MACH" == "aarch64" ]]; then 
        return
    fi

    RSYNC="/usr/local/bin/udr rsync"

    # Download my own statically compiled udr binary
    if [[ ! -f /usr/local/bin/udr ]]; then
      echo2 'Downloading download-tool udr (UDP-based rsync with multiple streams) to /usr/local/bin/udr'
      waitKey
      downloadFile $UDRURL > /usr/local/bin/udr
      chmod a+x /usr/local/bin/udr
    fi
}

function cleanTrash () 
{
    echo2 Removing files older than one day in $TRASHDIR, not running on $TRASHDIR/ct
    # -L = follow symlinks
    # -atime +1 = files older than one day
    find -L $TRASHDIR -not -path $TRASHDIR/ct/\* -and -type f -atime +1 -exec rm -f {} \;
}

function updateBlatServers ()
{
   echo2 Creating or updating the BLAT servers table
   downloadFile http://$HGDOWNLOAD/admin/hgcentral.sql | $MYSQL hgcentral
   # the blat servers don't have fully qualified dom names in the download data
   $MYSQL hgcentral -e 'UPDATE blatServers SET host=CONCAT(host,".soe.ucsc.edu");'
   # just in case that we ever add fully qualified dom names
   $MYSQL hgcentral -e 'UPDATE blatServers SET host=replace(host,".soe.ucsc.edu.soe.ucsc.edu", ".soe.ucsc.edu");'
}

function cgiUpdate ()
{
   # update the CGIs
   $RSYNC -avzP --exclude=RNAplot --exclude=hg.conf --exclude=hg.conf.local --exclude=RNAplot $HGDOWNLOAD::cgi-bin/ $APACHEDIR/cgi-bin/ 
   # update the html docs
   echo2 Updating Apache htdocs
   $RSYNC -avzP --exclude=*.{bb,bam,bai,bw,gz,2bit,bed} --exclude=ENCODE --exclude=trash $HGDOWNLOAD::htdocs/ $APACHEDIR/htdocs/ 
   # assign all downloaded files to a valid user. 
   chown -R $APACHEUSER:$APACHEUSER $APACHEDIR/*
   updateBlatServers
}

function updateBrowser {
   echo
   cgiUpdate

   DBS=$*
   # if none specified, update all
   if [ "$DBS" == "" ] ; then
       DBS=`ls $GBDBDIR/`
   fi

   # update gbdb
   echo updating GBDB: $DBS
   for db in $DBS; do 
       echo2 syncing gbdb: $db
       rsync -avp $RSYNCOPTS $HGDOWNLOAD::gbdb/$db/ $GBDBDIR/$db/ 
   done

   # update the mysql DBs
   stopMysql
   DBS=`ls /var/lib/mysql/ | egrep -v '(Trash$)|(hgTemp)|(^ib_)|(^ibdata)|(^aria)|(^mysql)|(performance)|(.flag$)|(multi-master.info)|(sys)|(lost.found)|(hgcentral)'`
   for db in $DBS; do 
       echo2 syncing full mysql database: $db
       $RSYNC --update --progress -avp $RSYNCOPTS $HGDOWNLOAD::mysql/$db/ $MYSQLDIR/$db/
   done
   startMysql

   mysqlCheck

   hideSomeTracks

   echo2 update finished
}

function addTools {
   rsync -avP hgdownload.soe.ucsc.edu::genome/admin/exe/linux.x86_64/ /usr/local/bin/
   rm -rf /usr/local/bin/blat.tmp # in case an old one is still there
   mv /usr/local/bin/blat /usr/local/bin/blat.tmp # tools under the BLAT license are separated into their own directory
   mv /usr/local/bin/blat.tmp/* /usr/local/bin/
   rmdir /usr/local/bin/blat.tmp
   echo2 The UCSC User Tools were copied to /usr/local/bin
   echo2 Please note that most of the tools require an .hg.conf file in the users
   echo2 home directory. A very minimal .hg.conf file can be found here:
   echo2 "http://genome-source.soe.ucsc.edu/gitlist/kent.git/blob/master/src/product/minimal.hg.conf"
}

# ------------ end of utility functions ----------------

# -- START OF SCRIPT  --- MAIN ---

# On Debian and OSX, sudo by default does not update the HOME variable (hence the -H option above)
if [[ "${SUDO_USER:-}" != "" ]]; then
   export HOME=~root
fi

trap errorHandler ERR

# OPTION PARSING

# show help message if no argument is specified
if [[ $# -eq 0 ]] ; then
   echo "$HELP_STR"
   exit 0
fi

while getopts ":baeut:hof" opt; do
  case $opt in
    h)
      echo "$HELP_STR"
      exit 0
      ;;
    b)
      function waitKey {
          echo
      }
      ;;
    a)
      HGDOWNLOAD=hgdownload-euro.soe.ucsc.edu
      ;;
    t)
      val=${OPTARG}
      # need to include all subdirectories for include to work
      # need to exclude everything else for exclude to work
      if [[ "$val" == "bestEncode" ]]; then
          RSYNCOPTS="-m --include=wgEncodeGencode* --include=wgEncodeBroadHistone* --include=wgEncodeReg* --include=wgEncodeAwg* --include=wgEncode*Mapability* --include=*/ --exclude=wgEncode*"
          ONLYGENOMES=0
      elif [[ "$val" == "noEncode" ]]; then
          RSYNCOPTS="-m --include=wgEncodeGencode* --include=*/ --exclude=wgEncode*"
          ONLYGENOMES=0
      elif [[ "$val" == "main" ]]; then
          # gbCdnaInfo
          # SNP table selection explained in #17335
          RSYNCOPTS="-m --include=grp.* --include=*gold* --include=augustusGene.* --include=chromInfo.* --include=cpgIslandExt.* --include=cpgIslandExtUnmasked.* --include=cytoBandIdeo.* --include=genscan.* --include=microsat.* --include=simpleRepeat.* --include=tableDescriptions.* --include=ucscToINSDC.* --include=windowmaskerSdust.*  --include=gold.* --include=chromInfo.* --include=trackDb* --include=hgFindSpec.* --include=gap.* --include=*.2bit --include=html/description.html --include=refGene* --include=refLink.* --include=wgEncodeGencode* --include=snp146Common* --include=snp130* --include=snp142Common* --include=snp128* --include=gencode* --include=rmsk* --include=*/ --exclude=*"
          ONLYGENOMES=1 # do not download hgFixed,go,proteome etc
      else
          echo "Unrecognized -t value. Please read the help message, by running bash $0 -h"
          exit 100
      fi
      ;;
    u)
      checkDownloadUdr
      ;;
    o)
      if [ ! -f $APACHEDIR/cgi-bin/hg.conf ]; then
         echo Please install a browser first, then switch the data loading mode.
      fi

      goOffline
      echo $APACHEDIR/cgi-bin/hg.conf was modified. 
      echo Offline mode: data is loaded only from the local Mysql database and file system.
      echo Use the parameter -f to switch to on-the-fly mode.
      exit 0
      ;;
    f)
      if [ ! -f $APACHEDIR/cgi-bin/hg.conf ]; then
         echo Please install a browser first, then switch the data loading mode.
	 exit 0
      fi

      goOnline
      echo $APACHEDIR/cgi-bin/hg.conf was modified. 
      echo On-the-fly mode activated: data is loaded from UCSC when not present locally.
      echo Use the parameter -o to switch to offline mode.
      exit 0
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      ;;
  esac
done
# reset the $1, etc variables after getopts
shift $(($OPTIND - 1))

# detect the OS version, linux distribution
unameStr=`uname`
DIST=none
MACH=`uname -m`

if [[ "$unameStr" == MINGW32_NT* ]] ; then
    echo Sorry Windows/CYGWIN is not supported
    exit 100
fi

# set a few very basic variables we need to function
if [[ "$unameStr" == Darwin* ]]; then
    OS=OSX
    DIST=OSX
    VERNUM=`sw_vers -productVersion`
    VER=$VERNUM
    SEDINPLACE="sed -Ei .bak" # difference BSD vs Linux

    # Not used anymore, but good to know that these are possible:
    #APACHECONFDIR=$APACHEDIR/ext/conf # only used by the OSX-spec part
    #APACHECONF=$APACHECONFDIR/001-browser.conf
    #APACHEUSER=_www # predefined by Apple
    #MYSQLDIR=$APACHEDIR/mysqlData
    #MYSQLUSER=_mysql # predefined by Apple
    #MYSQL="$APACHEDIR/ext/bin/mysql --socket=$APACHEDIR/ext/mysql.socket"
    #MYSQLADMIN="$APACHEDIR/ext/bin/mysqladmin --socket=$APACHEDIR/ext/mysql.socket"
    # make sure resulting binaries can be run on OSX 10.7
    # this is a gcc option, not a global variable for this script
    #export MACOSX_DEPLOYMENT_TARGET=10.7

elif [[ $unameStr == Linux* ]] ; then
    OS=linux
    if [ -f /etc/debian_version ] ; then
        DIST=debian  # Ubuntu, etc
        VER=$(cat /etc/debian_version)
        APACHECONF=/etc/apache2/sites-available/001-browser.conf
        APACHEUSER=www-data
    elif [[ -f /etc/redhat-release ]] ; then
        DIST=redhat
        VER=$(cat /etc/redhat-release)
        APACHECONF=/etc/httpd/conf.d/001-browser.conf
        APACHEUSER=apache
    elif [[ -f /etc/os-release ]]; then
        # line looks like this on Amazon AMI Linux: 'ID_LIKE="rhel fedora"'
        source /etc/os-release
        if [[ $ID_LIKE == rhel* ]]; then
                DIST=redhat
                VER=$VERSION
                APACHECONF=/etc/httpd/conf.d/001-browser.conf
                APACHEUSER=apache
        fi
    fi

    VERNUM=0
    # only works on redhats IMHO
    if [ -f /etc/system-release-cpe ] ; then
	VERNUM=`cut /etc/system-release-cpe -d: -f5`
    fi
    # os-release should work everywhere and has the full version number
    if [ -f /etc/os-release ] ; then
	VERNUM=`cat /etc/os-release | grep VERSION_ID | cut -d= -f2 | tr -d '"'`
    fi
fi

if [ "$DIST" == "none" ]; then
    echo Sorry, unable to detect your linux distribution. 
    echo Currently only Debian and Redhat-style distributions are supported.
    exit 100
fi

lastArg=${*: -1:1}
if [[ "$#" -gt "1" && ( "${2:0:1}" == "-" ) || ( "${lastArg:0:1}" == "-" )  ]]; then
  echo "Error: The options have to be specified before the command, not after it."
  echo
  echo "$HELP_STR"
  exit 100
fi

if [[ "$EUID" == "0" ]]; then
  if [[ "$OS" == "OSX" ]]; then
     echo "On OSX, this script must not be run with sudo, so we can install brew."
     echo "sudo -H $0"
     exit 100
  fi
else
  if [[ "$OS" != "OSX" ]]; then
     echo "On Linux, this script must be run as root or with sudo like below, so we can run apt/yum:"
     echo "sudo -H $0"
     exit 100
  fi
fi

if [ "${1:-}" == "install" ]; then
   installBrowser

elif [ "${1:-}" == "minimal" ]; then
   downloadMinimal ${@:2} # all arguments after the second one

elif [ "${1:-}" == "mirror" ]; then
   downloadGenomes ${@:2} # all arguments after the second one

elif [ "${1:-}" == "cgiUpdate" ]; then
   cgiUpdate

elif [ "${1:-}" == "update" ]; then 
   updateBrowser ${@:2} # all arguments after the second one

elif [ "${1:-}" == "clean" ]; then
    cleanTrash

elif [ "${1:-}" == "addTools" ]; then
    addTools

elif [ "${1:-}" == "mysql" ]; then
    mysqlDbSetup

elif [ "${1:-}" == "online" ]; then
    goOnline

elif [ "${1:-}" == "offline" ]; then
    goOffline

elif [ "${1:-}" == "dev" ]; then
    buildTree 

else
   echo Unknown command: ${1:-}
   echo "$HELP_STR"
   exit 100
fi
