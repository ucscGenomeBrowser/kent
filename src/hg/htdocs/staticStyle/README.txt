This file contains recommended conventions for CSS files used in the Genome Browser.

1. Use whitespace, naming and case conventions below:

.gbBlueDark {
    background-color: #003a72;
}

2. Avoid use of id's to style.  

3. Use CSS shortcuts when available:
         (e.g. padding 1 value (all), 2 values (top+bottom, left+right),
                3 (top, right+left, bottom), 4 (top, right, bottom, left) 

4. Group rules by type (e.g. header, footer), then alphabetically within group,
    unless cascade requires otherwise (overrides must be after rules they override)

5. Within a rule, group properties by type (positioning, display, color, text, other),
        and within the group, logically (e.g. padding, border, margin) or alphabetically

