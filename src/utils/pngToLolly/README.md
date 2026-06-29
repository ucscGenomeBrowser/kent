# pngToLolly

Render a PNG image as a UCSC Genome Browser "stemless lolly" bigLolly track:
the picture becomes a mosaic of colored dots, one dot per opaque pixel of the
downscaled image.  Each dot's column maps to a genomic position, its row to the
lolly value (vertical position), and its color to itemRgb.  Local image detail
("busyness") varies the dot size.

## Build

```
cd kent/src/utils/pngToLolly
make
```

The binary installs to `~/bin/$MACHTYPE/pngToLolly`.

## Run

```
pngToLolly in.png outDir
cd outDir && ./makeHub.sh
# move outDir under ~/public_html, then load the printed hubUrl
```

`makeHub.sh` runs `faToTwoBit`/`fetchChromSizes` and `bedToBigBed` to turn the
emitted text files into the `2bit`/`bigBed` the hub serves.

### Two output modes

* **Synthetic assembly (default).**  Emits a self-contained assembly hub on a
  blank one-chromosome genome (`chrImg` / `lollyImg`).  The image fills the whole
  synthetic chromosome.

* **Track hub on an existing assembly** (`-db` + `-pos`).  Lays the mosaic across
  a real genomic window, e.g. over a gene:

  ```
  pngToLolly -db=hg38 -pos=chr19:11087000-11136000 monaLisa.png outDir
  ```

  `makeHub.sh` then fetches the real `chrom.sizes` for the assembly.

### Viewing URL

The tool prints a ready-to-edit `hgTracks` URL.  To show only the mosaic plus,
say, the MANE track and hide everything else:

```
https://genome.ucsc.edu/cgi-bin/hgTracks?db=hg38&hubUrl=<pub>/outDir/hub.txt&position=chr19:11087000-11136000&pix=500&hideTracks=1&mane=pack&lollyImg=squish
```

Add `&udcTimeout=1` to defeat the browser's UDC cache while iterating (otherwise
a rebuilt `img.bb` at the same URL keeps serving the old data).

## Options

```
-maxDim=N     longest side of the image after downscaling (default 64).  More
              dots -> finer spatial detail.
-dotSize=N    on-screen dot diameter in pixels (default 8); also sets the track
              height, and therefore the on-screen aspect ratio (see below).
-sizeVar=N    neighborhood radius for measuring local variation (default 1).
              0 turns size variation off.
-sizeLo=F     smallest dot radius as a fraction of the tiling radius (default
              0.65); used for ordinarily-busy areas.
-sizeBusy=F   extra-small radius fraction for the very busiest areas (default
              0.4); set equal to sizeLo to disable.
-sizeHi=F     largest dot radius fraction (default 2.3); used for flat areas.
-invertSize   make busy areas big and flat areas small (default is the reverse).
-jitter=F     random horizontal nudge per dot, in fractions of a column
              (default 0.3); 0 turns it off.
-alpha=N      skip pixels whose alpha is below N (0-255, default 128).
-name=string  short label for the track (default derived from outDir).
-db=database  emit a track hub for this existing assembly instead of a synthetic
              one (requires -pos).
-pos=chr:s-e  genomic window the mosaic spans, when -db is set.
```

## Sizing notes (important)

The browser renders a lolly's radius as `lollySize * trackHeight / 100`, rounded
to a whole pixel, and `trackHeight` is fixed by the canvas width needed to keep
the picture's proportions.  Two consequences:

* **The smallest possible dot radius is about `pix / 67`.**  At `pix=1000` that
  is ~15 px; at `pix=500`, ~7.5 px; at `pix=240`, ~4 px.  To get small, crisp
  dots, use a smaller canvas (`pix`).  The bp width of the window does **not**
  affect dot size (dots are sized in pixels); only `pix` does.

* **Size variation is quantized** to steps of `trackHeight / 100` (~`pix/67` px).
  A large canvas gives few distinct dot sizes; a small canvas gives finer
  variation.  Generate with a `dotSize` matched to the `pix` you intend to view
  at (the printed URL uses `pix = maxDim_width * dotSize`); viewing at a very
  different `pix` distorts the proportions.
