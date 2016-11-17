This CGI is based on a Body Map illustration created by Jeff West Design.  The artwork is
provided in Adobe Illlustrator (.ai) and EPS formats, which must be exported to SVG format
for use in code.  There are also some minor edits applied to the SVG for code compatibility.
Instructions below.

In Adobe Illustrator:

(0. Set naming convention for elements (Preferences, Units, Identify Objects By: XML ID)

1. Open file (File Open from top menu)

2. Crop to minimize whitespace and remove title (Artboards tab in right panel, crop icon)

        Name: BodyMap_Draft04_Cropped (or whatever)

        Width: 620 pt, X: 125pt
        Height: 750 pt, Y: 80 pt

   NOTE:  the X, Y may need some tweaking -- use crop box viz to guide

    ** Alternatively, if OK to retain title in .svg, one can edit viewbox in SVG.
                  e.g. viewBox="55 45 620 730"
        (make sure Esophageal Junction is not truncated)

3. Save in SVG format (Save As, Format: SVG)

        * check Use Artboard ?
        - on SVG options, 
                * SVG 1.1
                * Fonts:  SVG, None
                * uncheck 'Preserve Illustrator Editing'
                * CSS Properties: Style Elements
                * check 'Output fewer <tspan> elements
                * check 'Responsive'
                * Decimal places: 1 (or 2 for higher res)
                (Fonts (SVG/Only Glyphs ? -- if need to include fonts in SVG)
                (Use <textpath>) -- n/a here  (e.g. curving text)
                ( Link ) -- n/a here (e.g. images embedded in illustration)

From script/editor:

1. Strip XML header and comment from Adobe (first line should be <svg>)

2. Clean up identifiers (replace _x5F_ with _ if needed.  Investigate/fix 
        text ids suffixed w/ _1_.
        
    sed -e 's/_x5F_/_/g' file.svg > bodyMap.svg
    mv bodyMap.svg ~/kent/src/hg/htdocs/images

3. Adjust viewbox if needed

55 45 620 730
(check aspect ratio)
        
4. Change fonts to match Genome Browser and generally work well with page:

        font-family:  Lato-Regular -> Lato,Arial,Helvetica
        font-size: 12.5px -> 11px 

5. Add CSS link to svg (just after <svg> element)

<defs>
    <link rel="stylesheet" href="../style/hgGtexTrackSettings.css" xmlns="http://www.w3.org/1999/xhtml"/>
</defs>

6. Remove Text_Lo group (or set CSS display:none)

7. Some id's will need stripping of terminal _1.  Unclear to me when/why these are added in cases where there's only 1!

* e.g. arteryAorta_Text_Hi_1_

8. Tweak style for leader line color if desired.  e.g. now set to paler (pink) on non-hover.

    was:  #EEC1C24
 e.g.   .st37{fill:none;stroke:#F69296;.....

9. Fix some bad id's if still present:

- arteryAorta_Pict_Lo
- arteryCoronary_Pic_lo

10. Remove empty <g>'s.. e.g. on Liver

11. Trim credit. (Remove 'ILLUSTRATION') paths

=== From Code review

12. Correction: first CerebelHemi_Pic_Hi group -> CerebelHemiPic_Lo

=== From QA

13. Tissue name tweaks to match gtexTissue table, where easily changed:

* Angerior cingulate cortex
* Substantia nigra
* Muscle - Skeletal
* EBV-transformed lymphocytes
* Transformed fibroblastso

14. Darker outline on Skin Suprapubic Pic_Hi (to match others)
        -> change stroke in rule .st170 to #231f20

15. Make breast visible (change its style (currently st92) to opacity .7, remove display:none


