---
title: "Genome Browser Documentation"
---

The Genome Browser offers a web-based interface for visualizing genomic data on
a variety of genome assemblies. If you're new here, the tutorials below are
the fastest way to get started.

## Tutorials

| Tutorial | Description |
| - | - |
| [Genome Browser Basics](/docs/tutorials/gb101.html) | Learn the basics of the Genome Browser interface |
| [Gateway Page](/docs/tutorials/gatewayTutorial.html) | Find and switch between genome assemblies |
| [Table Browser](/docs/tutorials/tableBrowserTutorial.html) | Query and download Genome Browser data |
| [Custom Tracks](/docs/tutorials/customTrackTutorial.html) | Display your own annotations in the Genome Browser |

Take a look at our [training slides](/docs/browserSlideDecks.html) for more walk-throughs of Genome Browser features, 
including cancer data and variant analysis.

Prefer video? Our [training page](/training/index.html) has short how-to
videos on common tasks like saving sessions, using BLAT and isPCR, and
finding SNPs in a gene.

Looking up a term you don't recognize? Check our
[glossary](/docs/genomeBrowserGlossary.html).

## Visualize your own data

Display your own annotations alongside the data we host:

- [Custom tracks](/cgi-bin/hgCustom) — the simpler option for displaying
  your own annotations. See our
  [documentation](/goldenPath/help/customTrack.html) to learn how to load
  your own.
- [Track hubs](/cgi-bin/hgHubConnect#unlistedHubs) — more powerful and
  configurable, ideal for many tracks or for sharing data publicly. Start
  with our [Hub Basics](/docs/hubs/hubBasics.html) page, and validate your
  hub with our [hub development tools](/cgi-bin/hgHubConnect#hubDeveloper).

For both hubs and custom tracks, we provide storage through
[hub space](/cgi-bin/hgHubConnect#hubUpload).

## Share your work

Once you've configured a view, share it with collaborators or publish it
broadly:

- [Links](/FAQ/FAQlink.html) — share a snapshot of your current view as a URL.
- [Sessions](/cgi-bin/hgSession) — save and share full Genome Browser
  configurations.
- [Public hubs](/cgi-bin/hgHubConnect#publicHubs) — to contribute your own,
  see our [guidelines](/goldenPath/help/publicHubGuidelines.html).
- [Contributed tracks](/docs/hubs/contributedTracks.html) — submit
  annotations on [GenArk](https://hgdownload.gi.ucsc.edu/hubs/) assemblies.

## Download data

Access our underlying data outside the browser:

- The [download server](https://hgdownload.gi.ucsc.edu/downloads.html)
  provides the data underlying the Genome Browser, plus pairwise alignments
  and LiftOver chain files.
- Our [REST API](/goldenPath/help/api.html) returns data in JSON format.
- [Command-line tools](https://hgdownload.gi.ucsc.edu/downloads.html#utilities_downloads)
  for working with bigBed, bigWig, and other formats, plus standalone
  versions of `liftOver`, `blat`, and others.

## Get help

- [Contact us](/contacts.html) — email, mailing lists, and other support
  channels.
- [FAQ](/FAQ/) — common questions, including
  [data file formats](/FAQ/FAQformat.html).
- Want a hands-on workshop at your institution?
  [Get in touch](/contacts.html) about hosting one, in-person or virtual.
- Found a bug or have an idea?
  [Submit a suggestion](/cgi-bin/hgUserSuggestion).
