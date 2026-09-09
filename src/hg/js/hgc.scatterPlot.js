// hgc.scatterPlot.js - ES6 module for drawing scatterplots on hgc details pages.
// Loaded via dynamic import() from the detailsScript trackDb mechanism.
//
// bedDetails.scripts.scatterPlot is an array of one entry per detailsScript setting.
// The entry that carries a dataUrl drives the plot:
//   {field, value, dataUrl, xLabel, yLabel, title, exportFields, fieldValues}
//
// dataUrl points at the background points. The file is not fetched from its URL
// directly: it is requested through hgTrackUi, which resolves it with udc and only
// serves a path that sits inside a hub attached to this cart, the same route the
// faceted composite uses for its metadata file. So the file has to live in the hub,
// a hub-relative path works even for a hub loaded from a local path, and no CORS
// header is needed on the host serving it. When the session has file caching turned
// off (bedDetails.udcTimeout, from the hgHubConnect button) the request is POSTed so
// the browser cannot answer it from its own cache. Accepted as JSON:
//   [[x,y], ...]
//   {"points": [[x,y], ...]}
//   {"points": [{"x":x, "y":y, "l":"mouseover label", "c":"category"}, ...]}
//   {"labels": ["cat A","cat B"], "points": [[x,y,catIndex,"mouseover label"], ...]}
// or as TSV, when the URL ends in .tsv or .txt, or when the body is not JSON:
//   x <TAB> y <TAB> l <TAB> c            (header required; l and c optional)
// Points carrying a category are colored by it and get a legend. The indexed JSON
// form exists because category names are long and repeat across thousands of points.
// Points carrying an "l" get a mouseover readout; without any l the plot has none.
//
// The highlighted points come from fieldValues, the bigBed fields named in the
// exportFields config key, each holding a coordinate pair as "(x,y)" or logfmt
// "x=<num> y=<num>". Their order follows exportFields. A track with a single
// highlighted point can leave exportFields out and put the pair in the field the
// setting names.

// The cloud goes on a canvas because these files hold thousands of points and that
// many <circle> elements make the details page crawl. Axes, labels and the few
// highlighted points stay SVG, overlaid, so they remain crisp.

const CLOUD_COLOR = "#b4b4b4";
const HIGHLIGHT_COLORS = ["#d40000", "#0033cc", "#00802b", "#cc6600", "#7700aa"];
const PLOT_W = 430, PLOT_H = 370;
const PAD_L = 55, PAD_R = 15, PAD_T = 12, PAD_B = 45;
const HIT_RADIUS = 6;     // how near the cursor must come to a point, in pixels
const HIT_CELL = 12;      // hit-index cell size, comfortably above HIT_RADIUS

// One fetch per URL per page, shared between entries naming the same file.
const cloudCache = new Map();

export function scatterPlot(bedDetails) {
    if (!bedDetails || !bedDetails.scripts || !bedDetails.scripts.scatterPlot)
        return;

    let entries = bedDetails.scripts.scatterPlot;
    // The plot is described by the first entry with a dataUrl. Other entries are
    // extra highlighted points, so their rows fold into the one plot.
    let main = entries.find(e => e && e.dataUrl);
    if (!main)
        return;

    let highlights = collectHighlights(entries);
    let host = prepareHost(entries, main);
    if (!host)
        return;

    host.textContent = "Loading plot…";

    loadCloud(main.dataUrl, bedDetails.track, bedDetails.udcTimeout).then(function (cloud) {
        render(host, cloud, highlights, main);
    }).catch(function (err) {
        // A missing or cross-origin-blocked data file must not take the page down:
        // the coordinates are still listed below, so say so quietly and stop.
        host.textContent = "Plot data could not be loaded";
        if (highlights.length)
            host.appendChild(buildLegend(null, highlights));
        if (window.console)
            console.warn("scatterPlot: could not load " + main.dataUrl, err);
    });
}

function collectHighlights(entries) {
    // Highlighted points, in the order the hub listed them in exportFields, then any
    // coordinate carried by an entry's own field value.
    let out = [];
    let seen = new Set();

    for (let entry of entries) {
        let fv = entry && entry.fieldValues;
        if (!fv)
            continue;
        // exportFields fixes the order; fall back to object order if it is absent.
        let names = Array.isArray(entry.exportFields) ? entry.exportFields : Object.keys(fv);
        for (let name of names) {
            if (seen.has(name) || !(name in fv))
                continue;
            let pt = parsePoint(fv[name]);
            if (pt) {
                out.push({name: name, x: pt.x, y: pt.y});
                seen.add(name);
            }
        }
    }

    // The setting's own field can hold a coordinate too, for a plot with a single
    // highlighted point and no exportFields at all.
    for (let entry of entries) {
        if (!entry || !entry.field || seen.has(entry.field))
            continue;
        let pt = parsePoint(entry.value);
        if (pt) {
            out.push({name: entry.field, x: pt.x, y: pt.y});
            seen.add(entry.field);
        }
    }
    return out;
}

function prepareHost(entries, main) {
    // Put the plot in the value cell of the row that named it, and drop the rows of the
    // other entries: their values become legend lines, so nothing is lost from the page.
    let mainRow = document.getElementById("bfld_" + main.field);
    if (!mainRow)
        return null;

    for (let entry of entries) {
        if (!entry || entry === main || !entry.field)
            continue;
        let row = document.getElementById("bfld_" + entry.field);
        if (row && row.parentNode)
            row.parentNode.removeChild(row);
    }

    let labelCell = mainRow.cells[0];
    if (labelCell)
        labelCell.textContent = main.title || "Scatterplot";
    return mainRow.cells[1] || null;
}

function loadCloud(url, track, noCache) {
    let key = track + "\t" + url;
    if (!noCache && cloudCache.has(key))
        return cloudCache.get(key);
    let wantTsv = /\.(tsv|txt)(\?|#|$)/i.test(url);

    // hgTrackUi checks the path against the hubs on this cart before reading it, so the
    // request carries the track it came from and the session id. It answers with
    // Cache-Control and an ETag, so repeat views are served from the browser cache.
    let body = "fileUrl=" + encodeURIComponent(url) +
               "&track=" + encodeURIComponent(track);
    let hgsid = new URLSearchParams(window.location.search).get("hgsid");
    if (hgsid !== null)
        body += "&hgsid=" + encodeURIComponent(hgsid);
    // POST when the session has caching turned off, because the browser may answer a
    // repeat GET from its own cache and never reach the CGI; also when the URL is too
    // long for a GET.
    let getUrl = "/cgi-bin/hgTrackUi?" + body;
    let req = (noCache || getUrl.length > 2048)
        ? fetch("/cgi-bin/hgTrackUi", {
              method: "POST",
              headers: {"Content-Type": "application/x-www-form-urlencoded"},
              body: body})
        : fetch(getUrl, {method: "GET"});

    let p = req.then(function (resp) {
        if (!resp.ok)
            throw new Error("HTTP " + resp.status);
        return resp.text();
    }).then(function (text) {
        if (!wantTsv) {
            try {
                return normalizePoints(JSON.parse(text));
            } catch (e) {
                // fall through: a mislabelled or extensionless TSV still works
            }
        }
        return parseTsv(text);
    });
    cloudCache.set(key, p);
    return p;
}

function makeCloud() {
    return {points: [], labels: [], hasLabels: false};
}

function catIndexer(cloud) {
    // Maps a category name to its index in cloud.labels, adding it on first sight.
    let seen = new Map();
    return function (name) {
        if (!name)
            return -1;
        if (!seen.has(name)) {
            seen.set(name, cloud.labels.length);
            cloud.labels.push(String(name));
        }
        return seen.get(name);
    };
}

function addPoint(cloud, x, y, cat, label) {
    if (!isFinite(x) || !isFinite(y))
        return;
    let lab = (label === undefined || label === null || label === "") ? null : String(label);
    if (lab !== null)
        cloud.hasLabels = true;
    cloud.points.push([x, y, cat, lab]);
}

function normalizePoints(data) {
    // Returns {points: [[x,y,catIndex,label|null], ...], labels: [...], hasLabels}
    let raw = Array.isArray(data) ? data : (data && data.points);
    if (!Array.isArray(raw))
        throw new Error("no point array in data file");

    let cloud = makeCloud();
    // A supplied label list fixes the category order; object form discovers it.
    if (data && Array.isArray(data.labels))
        cloud.labels = data.labels.map(String);
    let indexOf = catIndexer(cloud);

    for (let r of raw) {
        if (Array.isArray(r)) {
            let cat = -1;
            if (r.length > 2) {
                let n = Number(r[2]);
                if (Number.isInteger(n) && n >= 0 && n < cloud.labels.length)
                    cat = n;
            }
            addPoint(cloud, Number(r[0]), Number(r[1]), cat, r[3]);
        } else if (r && typeof r === "object") {
            addPoint(cloud, Number(r.x), Number(r.y), indexOf(r.c), r.l);
        }
    }
    return cloud;
}

function parseTsv(text) {
    let lines = text.split(/\r?\n/);
    let head = null, ix = {};
    let cloud = makeCloud();
    let indexOf = catIndexer(cloud);

    for (let line of lines) {
        if (line === "" || line.charAt(0) === "#")
            continue;
        let f = line.split("\t");
        if (head === null) {
            head = f.map(s => s.trim().toLowerCase());
            head.forEach((name, i) => { ix[name] = i; });
            if (!("x" in ix) || !("y" in ix))
                throw new Error("TSV needs x and y columns");
            continue;
        }
        addPoint(cloud, Number(f[ix.x]), Number(f[ix.y]),
                 indexOf("c" in ix ? f[ix.c] : null),
                 "l" in ix ? f[ix.l] : undefined);
    }
    if (head === null)
        throw new Error("empty data file");
    return cloud;
}

function parsePoint(str) {
    // "(0.433,-1.407)" or logfmt "x=0.433 y=-1.407". Returns null if neither.
    if (!str || typeof str !== "string")
        return null;
    let s = str.trim();

    let m = s.match(/^\(\s*(-?[0-9.eE+]+)\s*,\s*(-?[0-9.eE+]+)\s*\)$/);
    if (m) {
        let x = Number(m[1]), y = Number(m[2]);
        return (isFinite(x) && isFinite(y)) ? {x: x, y: y} : null;
    }

    // logfmt: take the first x= and the first y=, so x1=/y1= style keys work too.
    let xm = s.match(/(?:^|\s)x[0-9]*=(-?[0-9.eE+]+)/);
    let ym = s.match(/(?:^|\s)y[0-9]*=(-?[0-9.eE+]+)/);
    if (xm && ym) {
        let x = Number(xm[1]), y = Number(ym[1]);
        return (isFinite(x) && isFinite(y)) ? {x: x, y: y} : null;
    }
    return null;
}

function labelColor(i, n) {
    // Hues stepped in legend order, which the data file puts in cluster order, so
    // categories that sit together in the plot also sit together in hue.
    if (n <= 0 || i < 0)
        return CLOUD_COLOR;
    return "hsl(" + Math.round((360 * i) / n) + ", 62%, 47%)";
}

function niceBounds(points, highlights) {
    let xs = [], ys = [];
    for (let p of points) {
        xs.push(p[0]);
        ys.push(p[1]);
    }
    for (let h of highlights) {
        xs.push(h.x);
        ys.push(h.y);
    }
    if (xs.length === 0)
        return null;
    let x0 = Math.min(...xs), x1 = Math.max(...xs);
    let y0 = Math.min(...ys), y1 = Math.max(...ys);
    // A degenerate range would divide by zero; give it some width.
    if (x1 - x0 < 1e-9) { x0 -= 0.5; x1 += 0.5; }
    if (y1 - y0 < 1e-9) { y0 -= 0.5; y1 += 0.5; }
    let padX = (x1 - x0) * 0.05, padY = (y1 - y0) * 0.05;
    return {x0: x0 - padX, x1: x1 + padX, y0: y0 - padY, y1: y1 + padY};
}

function render(host, cloud, highlights, cfg) {
    let points = cloud.points, labels = cloud.labels;
    let b = niceBounds(points, highlights);
    if (!b) {
        host.textContent = "No data";
        return;
    }

    let innerW = PLOT_W - PAD_L - PAD_R;
    let innerH = PLOT_H - PAD_T - PAD_B;
    let sx = v => PAD_L + ((v - b.x0) / (b.x1 - b.x0)) * innerW;
    let sy = v => PAD_T + innerH - ((v - b.y0) / (b.y1 - b.y0)) * innerH;

    host.textContent = "";
    let wrap = document.createElement("div");
    wrap.style.position = "relative";
    wrap.style.width = PLOT_W + "px";
    wrap.style.height = PLOT_H + "px";

    // Screen coordinates once, reused by the drawing and by the hit index.
    let screen = points.map(p => [sx(p[0]), sy(p[1])]);

    // Cloud on canvas, at device resolution so the dots are not blurry on retina.
    let ratio = window.devicePixelRatio || 1;
    let canvas = document.createElement("canvas");
    canvas.width = PLOT_W * ratio;
    canvas.height = PLOT_H * ratio;
    canvas.style.width = PLOT_W + "px";
    canvas.style.height = PLOT_H + "px";
    canvas.style.position = "absolute";
    canvas.style.left = "0";
    canvas.style.top = "0";
    let ctx = canvas.getContext("2d");
    ctx.scale(ratio, ratio);
    // Group by category so the fill color is set once per category, not per point.
    let byCat = new Map();
    points.forEach(function (p, i) {
        let key = p[2];
        if (!byCat.has(key))
            byCat.set(key, []);
        byCat.get(key).push(i);
    });
    for (let [key, idxs] of byCat) {
        ctx.fillStyle = labelColor(key, labels.length);
        for (let i of idxs) {
            // fillRect beats arc() by a wide margin at this point count and is
            // indistinguishable at 2px.
            ctx.fillRect(screen[i][0] - 1, screen[i][1] - 1, 2, 2);
        }
    }
    wrap.appendChild(canvas);

    let svg = buildOverlay(b, sx, sy, highlights, cfg, innerW, innerH);
    wrap.appendChild(svg);

    if (cloud.hasLabels)
        attachHover(wrap, cloud, screen, labels);

    host.appendChild(wrap);
    host.appendChild(buildLegend(cloud, highlights));
}

function attachHover(wrap, cloud, screen, labels) {
    // Canvas cannot carry a <title>, so find the point under the cursor and show our
    // own readout. A cell index keeps the search local, so a big cloud stays smooth.
    let grid = new Map();
    let key = (cx, cy) => cx + "," + cy;
    screen.forEach(function (s, i) {
        if (cloud.points[i][3] === null)
            return;                 // only labelled points are findable
        let k = key(Math.floor(s[0] / HIT_CELL), Math.floor(s[1] / HIT_CELL));
        let cell = grid.get(k);
        if (cell)
            cell.push(i);
        else
            grid.set(k, [i]);
    });
    if (grid.size === 0)
        return;

    let tip = document.createElement("div");
    tip.style.cssText = "position:absolute; display:none; pointer-events:none; " +
        "background:#ffffe0; border:1px solid #999; padding:2px 5px; font-size:11px; " +
        "white-space:nowrap; z-index:10; box-shadow:1px 1px 3px rgba(0,0,0,0.25);";
    wrap.appendChild(tip);

    wrap.addEventListener("mousemove", function (ev) {
        let r = wrap.getBoundingClientRect();
        let mx = ev.clientX - r.left, my = ev.clientY - r.top;
        let gx = Math.floor(mx / HIT_CELL), gy = Math.floor(my / HIT_CELL);
        let best = -1, bestD = HIT_RADIUS * HIT_RADIUS;
        for (let dx = -1; dx <= 1; dx++)
            for (let dy = -1; dy <= 1; dy++)
                {
                let cell = grid.get(key(gx + dx, gy + dy));
                if (!cell)
                    continue;
                for (let i of cell)
                    {
                    let d = (screen[i][0] - mx) ** 2 + (screen[i][1] - my) ** 2;
                    if (d <= bestD)
                        {
                        bestD = d;
                        best = i;
                        }
                    }
                }
        if (best < 0) {
            tip.style.display = "none";
            return;
        }
        let p = cloud.points[best];
        let text = p[3];
        if (p[2] >= 0 && labels[p[2]] && labels[p[2]] !== text)
            text += " — " + labels[p[2]];
        tip.textContent = text;
        tip.style.display = "block";
        // Keep the tip inside the plot box so it does not overflow the details table.
        let left = screen[best][0] + 10;
        if (left + tip.offsetWidth > PLOT_W)
            left = Math.max(0, screen[best][0] - 10 - tip.offsetWidth);
        tip.style.left = left + "px";
        tip.style.top = Math.max(0, screen[best][1] - 22) + "px";
    });
    wrap.addEventListener("mouseleave", function () {
        tip.style.display = "none";
    });
}

function buildOverlay(b, sx, sy, highlights, cfg, innerW, innerH) {
    let ns = "http://www.w3.org/2000/svg";
    let svg = document.createElementNS(ns, "svg");
    svg.setAttribute("width", PLOT_W);
    svg.setAttribute("height", PLOT_H);
    // pointer-events off so the cloud underneath still sees the mouse; the highlight
    // circles turn it back on for their own tooltips.
    svg.setAttribute("style", "position:absolute; left:0; top:0; pointer-events:none; " +
                              "font-family:sans-serif; font-size:11px;");

    let add = (tag, attrs, text) => {
        let el = document.createElementNS(ns, tag);
        for (let k in attrs)
            el.setAttribute(k, attrs[k]);
        if (text !== undefined)
            el.textContent = text;
        svg.appendChild(el);
        return el;
    };

    // Grid and ticks
    for (let t of calcTicks(b.x0, b.x1)) {
        let x = sx(t);
        add("line", {x1: x, y1: PAD_T, x2: x, y2: PAD_T + innerH, stroke: "#eee"});
        add("line", {x1: x, y1: PAD_T + innerH, x2: x, y2: PAD_T + innerH + 4, stroke: "#666"});
        add("text", {x: x, y: PAD_T + innerH + 16, "text-anchor": "middle",
                     fill: "#333", "font-size": "10"}, fmtTick(t));
    }
    for (let t of calcTicks(b.y0, b.y1)) {
        let y = sy(t);
        add("line", {x1: PAD_L, y1: y, x2: PAD_L + innerW, y2: y, stroke: "#eee"});
        add("line", {x1: PAD_L - 4, y1: y, x2: PAD_L, y2: y, stroke: "#666"});
        add("text", {x: PAD_L - 6, y: y + 4, "text-anchor": "end",
                     fill: "#333", "font-size": "10"}, fmtTick(t));
    }

    // Axis lines
    add("line", {x1: PAD_L, y1: PAD_T, x2: PAD_L, y2: PAD_T + innerH, stroke: "#333"});
    add("line", {x1: PAD_L, y1: PAD_T + innerH, x2: PAD_L + innerW, y2: PAD_T + innerH,
                 stroke: "#333"});

    // Highlighted points, drawn last so they sit on top of the cloud. A white halo
    // and a dark ring keep them readable wherever they land in a colored cloud.
    highlights.forEach(function (h, i) {
        let color = HIGHLIGHT_COLORS[i % HIGHLIGHT_COLORS.length];
        let cx = sx(h.x), cy = sy(h.y);
        add("circle", {cx: cx, cy: cy, r: 8, fill: "#fff", "fill-opacity": "0.9"});
        let c = add("circle", {cx: cx, cy: cy, r: 5.5, fill: color,
                               stroke: "#000", "stroke-width": "1.5",
                               style: "pointer-events:auto"});
        let title = document.createElementNS(ns, "title");
        title.textContent = h.name + ": (" + h.x + ", " + h.y + ")";
        c.appendChild(title);
    });

    if (cfg.xLabel)
        add("text", {x: PAD_L + innerW / 2, y: PLOT_H - 6, "text-anchor": "middle",
                     fill: "#333", "font-size": "11"}, cfg.xLabel);
    if (cfg.yLabel)
        add("text", {x: 12, y: PAD_T + innerH / 2, "text-anchor": "middle",
                     fill: "#333", "font-size": "11",
                     transform: "rotate(-90 12 " + (PAD_T + innerH / 2) + ")"}, cfg.yLabel);
    return svg;
}

function legendLine(color, text) {
    let div = document.createElement("div");
    let s = document.createElement("span");
    s.style.color = color;
    s.textContent = "●";
    div.appendChild(s);
    div.appendChild(document.createTextNode(" " + text));
    return div;
}

function buildLegend(cloud, highlights) {
    // Built with DOM calls rather than innerHTML because the category names come from
    // a hub-authored data file.
    let box = document.createElement("div");
    box.style.fontSize = "11px";
    box.style.marginTop = "5px";
    box.style.maxWidth = PLOT_W + "px";

    highlights.forEach(function (h, i) {
        box.appendChild(legendLine(HIGHLIGHT_COLORS[i % HIGHLIGHT_COLORS.length],
                                   h.name + ": (" + h.x + ", " + h.y + ")"));
    });

    if (!cloud)
        return box;

    if (cloud.labels.length > 0) {
        let cats = document.createElement("div");
        cats.style.columnCount = "2";
        cats.style.columnGap = "12px";
        cats.style.marginTop = "4px";
        cloud.labels.forEach(function (name, i) {
            cats.appendChild(legendLine(labelColor(i, cloud.labels.length), name));
        });
        box.appendChild(cats);
    } else {
        box.appendChild(legendLine(CLOUD_COLOR,
            cloud.points.length.toLocaleString() + " background points"));
    }
    return box;
}

function calcTicks(lo, hi) {
    let span = hi - lo;
    if (!(span > 0))
        return [];
    let step = Math.pow(10, Math.floor(Math.log10(span)));
    let n = span / step;
    if (n < 2)
        step /= 5;
    else if (n < 5)
        step /= 2;
    let ticks = [];
    for (let v = Math.ceil(lo / step) * step; v <= hi + step * 1e-9; v += step)
        ticks.push(Math.abs(v) < step * 1e-9 ? 0 : v);
    return ticks;
}

function fmtTick(v) {
    let a = Math.abs(v);
    if (a !== 0 && (a < 0.01 || a >= 100000))
        return v.toExponential(1);
    return String(Math.round(v * 1000) / 1000);
}
