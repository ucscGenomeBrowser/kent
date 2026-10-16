---
title: "hubtools: Command-line Tool for Track Hubs"
---

`hubtools` is a command-line program for building and uploading track hubs. Its
main use is uploading a hub to [Hub Space](/cgi-bin/hgHubConnect#hubUpload), the free
storage that UCSC offers for track hubs. The web upload form works well for a few files,
but if your hub has dozens of files in several directories, or if you rebuild it
regularly, `hubtools up` is easier: it sends a whole directory in one command, and when
you run it again, it only uploads the files that changed.

hubtools can also write a hub.txt for a directory of bigBed and bigWig files, and create
a hub from an IGV session or from a UCSC Genome Browser session, which is useful when
you move data between the two browsers or want to turn your custom tracks into a hub.

1. [Quick start](#quick-start)
2. [Installation](#installation)
3. [Creating a hub from a directory of files](#creating-a-hub-from-a-directory-of-files)
4. [Uploading files to Hub Space](#uploading-files-to-hub-space)
5. [Updating a hub](#updating-a-hub)
6. [Importing an IGV session](#importing-an-igv-session)
7. [Importing a UCSC session or custom tracks](#importing-a-ucsc-session-or-custom-tracks)
8. [Other commands](#other-commands)

## Quick start

1. Put all your bigBed and bigWig files into one directory, here called `myHub`.
2. Get an API key: log in to the Genome Browser, open My Data > Track Hubs >
   [Hub Development](/cgi-bin/hgHubConnect#hubDeveloper) and click "Generate key".
   Save it like this:
   ```
   echo 'apiKey=xxxxxxxxxxxx' >> ~/.hubtools.conf
   ```
3. Download hubtools, then build the hub and upload it. Replace `hg38` with your assembly
   and `myHub` with any short name for the hub:
   ```
   curl -O https://hgdownload.soe.ucsc.edu/admin/exe/linux.x86_64/hubtools
   chmod a+x hubtools
   cd myHub
   ../hubtools build hg38
   ../hubtools up myHub
   ```
   Download the script outside of `myHub`, as above, otherwise `up` uploads it together with
   your data.
4. Open the [Hub Upload](/cgi-bin/hgHubConnect#hubUpload) tab. Your hub is in the list
   there, with a link to view it in the Genome Browser.

`build` only makes tracks for bigBed and bigWig files. BAM, CRAM and VCF files in the
directory are uploaded too, but you have to add a track for each of them to the hub.txt
yourself, see [Track Hub Basics](hubBasics.html).

## Installation

hubtools is a single Python 3 script that uses only the standard library, so there is
nothing to install besides Python itself. It runs on Linux and macOS. Download it and
make it executable:

```
curl -O https://hgdownload.soe.ucsc.edu/admin/exe/linux.x86_64/hubtools
chmod a+x hubtools
./hubtools -h
```

Put it into a directory on your `PATH`, e.g. `~/bin`, if you want to run it from
anywhere. `hubtools -h` lists all commands and `hubtools <command> -h` (e.g.
`hubtools up -h`) shows the options of one of them.

The commands that convert files (`import igv`, `import session`, `export bigbed` and
`splitHap`) also need UCSC command-line tools like `bedToBigBed`, `wigToBigWig` or
`gff3ToGenePred`. You do not have to install these yourself. If a tool is not on your
`PATH`, hubtools downloads it for your operating system (Linux or macOS, Intel or ARM)
from our [utilities directory](https://hgdownload.soe.ucsc.edu/admin/exe/) into
`~/.local/bin`, and prints a message saying so. This happens only the first time a tool is
needed. Uploading with `hubtools up` does not need any of these tools.

## Creating a hub from a directory of files

If you have a directory of bigBed and bigWig files and no hub.txt yet, `hubtools build`
writes one for you. Give it the assembly the files are on:

```
cd myHub/
hubtools build hg38
```

The assembly can be a UCSC name like `hg38` or `mm39`, or a GenBank/RefSeq accession like
`GCF_000001405.40` for one of the assemblies on [GenArk](https://hgdownload.soe.ucsc.edu/hubs/).
The result is a single hub.txt with `useOneFile on`, which contains the hub, the genome and
all tracks. You can load it right away from a web server or upload the whole directory with
`hubtools up`.

How the files become tracks:

- Every bigBed (`.bb`, `.bigBed`) and bigWig (`.bw`, `.bigWig`) file in the directory
  becomes one track. Other files are ignored.
- Every subdirectory becomes a composite track that contains the files in it. Deeper
  subdirectories are not used.
- The part of the file name before the first dot names the track, so `liver.rep1.bw`
  becomes the track `liver` with the short label "liver". This is also the name to use in
  the tracks.tsv file described below.
- If most files in a subdirectory come in pairs, a bigBed and a bigWig with the same name
  (e.g. `liver.peaks.bb` and `liver.signal.bw`), the composite gets two views, one for
  the peaks and one for the signal.

To set labels, colors or any other [trackDb setting](/goldenPath/help/trackDb/trackDbHub.html),
put a file called `tracks.tsv` into the directory. The first column is the track name and
the other columns are settings:

```
track	shortLabel	longLabel	color
liver	Liver	Liver ATAC-seq, replicate 1	200,0,0
brain	Brain	Brain ATAC-seq, replicate 1	0,0,200
```

The tracks are written in the order of this file. A `tracks.json`, `tracks.yaml` or
`tracks.ra` file works the same way. In these, the special key `.hub` sets the name,
labels and contact email of the hub itself:

```
{
    ".hub" : { "hub": "liverAtac", "shortLabel": "Liver ATAC-seq", "email": "you@example.edu" },
    "liver" : { "shortLabel": "Liver", "color": "200,0,0" }
}
```

A subdirectory can have its own tracks.tsv for the tracks of its composite. You can also
set your email once for all hubs with a line `email=you@example.edu` in `~/.hubtools.conf`.

When you add files later, run `hubtools build` again. Changes that you made by hand in the
hub.txt, like labels, colors, visibility or any setting that hubtools does not write itself,
are kept. With `-o`, the hub.txt is written to another directory than the one with the files.

## Uploading files to Hub Space

### Get an API key

hubtools logs into Hub Space with an API key instead of your password. To create one,
log in to the Genome Browser, go to My Data > Track Hubs and open the
[Hub Development](/cgi-bin/hgHubConnect#hubDeveloper) tab. Click "Generate key", then copy
the key into the file `~/.hubtools.conf`:

```
echo 'apiKey=xxxxxxxxxxxx' >> ~/.hubtools.conf
```

You only have to do this once per computer. Generating a new key revokes the old one, so
if you use hubtools on several machines, copy the same key to all of them.

### Upload a directory

Change into the directory with your hub and run `hubtools up` with a name for the hub:

```
cd myHub/
hubtools up atacSeq2024
```

This uploads every file in the directory and all its subdirectories. The hub name can be
any short string made of letters, digits, dots and underscores, e.g. `atacSeq2024` or
`muscle_rna`. Hub Space puts all files under a directory of that name, and the
subdirectories of your local copy are kept, so a `trackDb` or `bigDataUrl` line that
points to `bigWigs/sample1.bw` still works after the upload. Files and directories that
start with a dot are skipped.

Instead of changing into the directory, you can also give it with `-i`:

```
hubtools up atacSeq2024 -i ~/projects/atac/hub
```

To upload only some files, name them after the hub name. They are relative to the hub
directory and have to be inside it:

```
hubtools up atacSeq2024 hub.txt bigWigs/sample1.bw
```

Once the upload is done, the hub appears in the file list on the
[Hub Upload](/cgi-bin/hgHubConnect#hubUpload) tab. From there you can open it in the
Genome Browser or copy the URL of its hub.txt to share it.

### hub.txt and the assembly

hubtools reads the `hub.txt` (or a single file called `<something>.hub.txt`) in the hub
directory to find out which assembly each file belongs to. Both kinds of hubs work: a
single hub.txt with `useOneFile on`, and the classic layout with a separate genomes.txt
and one trackDb.txt per assembly. For a hub with only one assembly, every uploaded file is
assigned to it. For hubs with several assemblies, each file gets the assembly of the
`genome` stanza that refers to it.

If there is no hub.txt, the upload still works and Hub Space writes a hub.txt for you,
like it does for uploads from the web page. hubtools then prints a warning, because
without a hub.txt it cannot know the assembly, so the links in the Hub Space file list
will not open the right genome.

It is better to upload a hub.txt together with the data files. If you do not have one yet,
run [`hubtools build`](#creating-a-hub-from-a-directory-of-files) on the directory first,
or write one by hand following our [Track Hub Basics](hubBasics.html) page.

## Updating a hub

To update a hub, run the same command again with the same hub name. hubtools remembers
the modification time of every file it uploaded, in a file called `.hubtools.files.json`
in the hub directory. On the next run it skips files that have not changed and only uploads
new or modified ones, which saves a lot of time when the hub contains large bigWig
or BAM files.

```
hubtools up atacSeq2024
```

If a file has to be sent again even though it has not changed locally, for example
because you deleted it on Hub Space, use `-f` to ignore this record and upload everything:

```
hubtools up atacSeq2024 -f
```

Note that using a different hub name creates a second, separate hub. hubtools does not
delete files on Hub Space that you have deleted locally; remove those on the
[Hub Upload](/cgi-bin/hgHubConnect#hubUpload) tab.

## Importing an IGV session

`hubtools import igv` converts an IGV session file (the `.xml` file that IGV writes when
you save a session) into a track hub. The session can be a local file or a URL:

```
hubtools import igv mySession.xml -o igvHub
hubtools import igv https://ftp.example.edu/igv.myAssembly.xml --db hg38 -o igvHub
```

Every track of the session becomes a track in the hub, in the same order. IGV display
settings like color, data range, autoscale and windowing function are translated to the
matching trackDb settings.

Files that the Genome Browser can read over the network (bigWig, bigBed, BAM, CRAM,
.hic and tabix-indexed VCF) are linked at their current location. Text files like BED,
GFF, GTF, WIG and bedGraph cannot be used in a hub directly, so hubtools downloads them
into the output directory and converts them to bigBed or bigWig. For this it needs the
chromosome sizes. If the assembly is a UCSC assembly like hg38, these are downloaded
from UCSC. Otherwise they are taken from a bigWig or bigBed file in the session, or you
can provide a chrom.sizes file with `--chromSizes`.

The `genome` attribute of an IGV session is often a name that the Genome Browser does not
know. In that case, set the assembly with `--db`. This can be a UCSC name like `hg38` or a
GenBank/RefSeq accession like `GCF_048771995.1`, which the browser loads from
[GenArk](https://hgdownload.soe.ucsc.edu/hubs/).

Other options:

- `--download` also copies the bigWig, bigBed and BAM files, so the hub no longer depends
  on the original server
- `--noConvert` skips all text files, which gives you a quick look at the track structure
  without downloading anything

After the import, you can upload the result with `hubtools up myHub -i igvHub`.

## Importing a UCSC session or custom tracks

`hubtools import session` turns the custom tracks of a Genome Browser session into a track
hub. Custom tracks are kept on our servers only for a limited time, while a hub is a set of
files under your control, so this is a good way to keep tracks that you want to use for
longer, or to move them to Hub Space. The command needs the API key described above.

It accepts three kinds of input:

- a link to a saved session, as shown on the [My Sessions](/cgi-bin/hgSession) page
- any Genome Browser link that contains an `hgsid=` parameter, e.g. copied from the
  address bar
- a `.tar.gz` backup archive of your custom tracks, which you can create under My Data >
  My Sessions

```
hubtools import session 'https://genome.ucsc.edu/cgi-bin/hgTracks?db=hg38&hgsid=3458_8d3Mpu' -o myHub
hubtools import session SC_20230723_backup.tar.gz -o myHub
```

Put the URL in single quotes, otherwise the shell interprets the `&`. Text custom tracks are
converted to bigBed or bigWig files. Tracks that already use a `bigDataUrl` keep pointing to
the original file; add `--download` to copy those files into the hub as well.

The result is a single `hub.txt` with one `genome` stanza for every assembly that had custom
tracks, and the data files for each assembly in a subdirectory named after it, e.g.
`myHub/hg38/`. To set the contact email that goes into the generated hub.txt, add a line
`email=you@example.edu` to `~/.hubtools.conf`.

## Other commands

hubtools has a few more commands. Run each with `-h` for details.

- `hubtools export tsv hub.txt` converts a hub.txt to a tab-separated table, one track per
  line, which is easier to edit in a spreadsheet. Saved as `tracks.tsv`, it can then be fed
  back into `hubtools build`.
- `hubtools export bigbed <assembly>` converts .tsv files to bigBed files.
- `hubtools import jbrowse2 <url> <assembly>` creates a hub from a JBrowse2 installation.
- `hubtools tdb add`, `tdb nest` and `tdb unnest` edit the track structure of an existing
  hub.txt: they create composite and super tracks and move tracks into or out of them.
- `hubtools splitHap` splits a hub that was made on a diploid assembly into one hub with a
  separate genome for each haplotype.

For questions or problems, write to our
[mailing list](mailto:genome@soe.ucsc.edu) or see the
[Track Hub help page](/goldenPath/help/hgTrackHubHelp.html).
