// draws the pages hubSpaceReport.py writes, which define DATA before loading this file

const PAGE_MARKUP = `
<h1>Hub Space usage</h1>
<p class="sub" id="subtitle"></p>

<div class="tiles" id="tiles"></div>

<h2>Over time</h2>
<div class="grid3">
  <div class="card"><h3>Storage used</h3><div class="chart" id="chartDisk"></div></div>
  <div class="card"><h3>Uploaded files</h3><div class="chart" id="chartFiles"></div></div>
  <div class="card"><h3>Users with uploads</h3><div class="chart" id="chartUsers"></div></div>
</div>

<h2>Storage by file format</h2>
<div class="grid2">
  <div class="card"><h3>Over time</h3><div class="legend" id="areaLegend"></div>
    <div class="chart" id="chartArea"></div></div>
  <div class="card"><h3 id="barTitle">Latest report</h3><div class="chart" id="chartBars"></div></div>
</div>

<h2>Latest report</h2>
<div class="grid2">
  <div class="card"><h3>Files by kind</h3><div class="scroll" id="tableCategories"></div></div>
  <div class="card"><h3>Housekeeping</h3><div class="scroll" id="tableHealth"></div>
    <p class="note">These should all be zero. Anything else is worth a look in the full report.</p></div>
</div>

<h2>All reports</h2>
<div class="card">
  <details><summary>Show the numbers from every report</summary>
    <div class="scroll" id="tableAll"></div></details>
</div>
<p class="note">Storage used is du on the upload directory. The format sizes add up the
size of each file, so their total can differ a little from du.</p>
`;
document.getElementById("report").innerHTML = PAGE_MARKUP;

const SVGNS = "http://www.w3.org/2000/svg";
const MONTHS = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov",
                "Dec"];
const UNITS = ["B", "KB", "MB", "GB", "TB", "PB"];
const SLOTS = ["var(--series-1)", "var(--series-2)", "var(--series-3)", "var(--series-4)",
               "var(--series-5)"];
const OTHER = "var(--series-other)";
const TOP_FORMATS = SLOTS.length;

const reports = DATA.reports.map(r => Object.assign({}, r, {when: new Date(r.date)}));
const latest = reports[reports.length - 1];
const previous = reports.length > 1 ? reports[reports.length - 2] : null;
const withFormats = reports.filter(r => r.formats);

function fmtBytes(b) {
    let i = 0;
    while (b >= 1024 && i < UNITS.length - 1) {
        b /= 1024;
        i++;
    }
    if (i === 0)
        return b.toFixed(0) + " B";
    return (b < 10 ? b.toFixed(1) : b.toFixed(0)) + " " + UNITS[i];
}

function fmtInt(n) {
    return Math.round(n).toLocaleString("en-US");
}

function fmtDate(d, withYear) {
    let s = MONTHS[d.getMonth()] + " " + d.getDate();
    if (withYear)
        s += ", " + d.getFullYear();
    return s;
}

function fmtSigned(n, fmt) {
    if (n === 0)
        return "no change";
    return (n > 0 ? "+" : "−") + fmt(Math.abs(n));
}

function svgEl(tag, attrs, parent) {
    const e = document.createElementNS(SVGNS, tag);
    for (const k in attrs)
        e.setAttribute(k, attrs[k]);
    if (parent)
        parent.appendChild(e);
    return e;
}

function htmlEl(tag, text, parent, cls) {
    const e = document.createElement(tag);
    if (text !== undefined && text !== null)
        e.textContent = text;
    if (cls)
        e.className = cls;
    if (parent)
        parent.appendChild(e);
    return e;
}

// tooltip shared by every chart, filled with textContent only
const tip = document.getElementById("tip");
function showTip(ev, head, rows) {
    tip.textContent = "";
    htmlEl("div", head, tip, "head");
    for (const r of rows) {
        const row = htmlEl("div", r.text, tip, r.color ? "row" : "row plain");
        if (r.color)
            row.style.setProperty("--swatch", r.color);
    }
    tip.style.display = "block";
    const pad = 14;
    let x = ev.clientX + pad;
    let y = ev.clientY + pad;
    if (x + tip.offsetWidth > window.innerWidth - 8)
        x = ev.clientX - tip.offsetWidth - pad;
    if (y + tip.offsetHeight > window.innerHeight - 8)
        y = ev.clientY - tip.offsetHeight - pad;
    tip.style.left = Math.max(8, x) + "px";
    tip.style.top = Math.max(8, y) + "px";
}
function hideTip() {
    tip.style.display = "none";
}

// round tick steps, in 1024 units for bytes so the labels come out as whole GB or TB
function yTicks(max, isBytes) {
    if (max <= 0)
        max = 1;
    let unit = 1;
    if (isBytes)
        while (unit * 1024 <= max)
            unit *= 1024;
    const scaled = max / unit;
    const raw = scaled / 4;
    const pow = Math.pow(10, Math.floor(Math.log10(raw)));
    let step = pow;
    for (const m of [1, 2, 2.5, 5, 10]) {
        step = m * pow;
        if (step >= raw)
            break;
    }
    if (!isBytes && step < 1)
        step = 1;
    const ticks = [];
    for (let v = 0; v <= scaled + step * 0.999; v += step)
        ticks.push(v * unit);
    return ticks;
}

// the frame every time chart shares: scales, gridlines and axis labels
function timeFrame(host, times, yMax, isBytes, fmtY) {
    host.textContent = "";
    const w = Math.max(host.clientWidth, 260);
    const h = 220;
    const m = {l: 58, r: 14, t: 10, b: 26};
    const svg = svgEl("svg", {width: w, height: h, viewBox: "0 0 " + w + " " + h}, host);
    let t0 = Math.min(...times);
    let t1 = Math.max(...times);
    if (t1 - t0 < 86400000) {
        t0 -= 3 * 86400000;
        t1 += 3 * 86400000;
    }
    const ticks = yTicks(yMax, isBytes);
    const top = ticks[ticks.length - 1];
    const x = t => m.l + (t - t0) / (t1 - t0) * (w - m.l - m.r);
    const y = v => h - m.b - v / top * (h - m.t - m.b);
    const axis = svgEl("g", {class: "axis"}, svg);
    for (const v of ticks) {
        svgEl("line", {x1: m.l, x2: w - m.r, y1: y(v), y2: y(v), class: v === 0 ? "base" : ""},
              axis);
        const label = svgEl("text", {x: m.l - 8, y: y(v) + 4, "text-anchor": "end"}, axis);
        label.textContent = fmtY(v);
    }
    const withYear = new Date(t0).getFullYear() !== new Date(t1).getFullYear();
    const n = Math.max(2, Math.floor((w - m.l - m.r) / 110));
    for (let i = 0; i < n; i++) {
        const t = t0 + (t1 - t0) * (i + 0.5) / n;
        const label = svgEl("text", {x: x(t), y: h - 6, "text-anchor": "middle"}, axis);
        label.textContent = fmtDate(new Date(t), withYear);
    }
    return {svg, x, y, w, h, m};
}

// index of the report closest in time to the pointer
function nearest(f, times, ev) {
    const box = f.svg.getBoundingClientRect();
    const px = ev.clientX - box.left;
    let best = 0;
    for (let i = 1; i < times.length; i++)
        if (Math.abs(f.x(times[i]) - px) < Math.abs(f.x(times[best]) - px))
            best = i;
    return best;
}

function hoverLayer(f, times, onHover) {
    const cross = svgEl("line", {y1: f.m.t, y2: f.h - f.m.b, stroke: "var(--axis)",
                                 "stroke-width": 1, visibility: "hidden"}, f.svg);
    const hit = svgEl("rect", {x: f.m.l, y: 0, width: f.w - f.m.l - f.m.r,
                               height: f.h, fill: "transparent"}, f.svg);
    hit.addEventListener("mousemove", ev => {
        const i = nearest(f, times, ev);
        cross.setAttribute("x1", f.x(times[i]));
        cross.setAttribute("x2", f.x(times[i]));
        cross.setAttribute("visibility", "visible");
        onHover(ev, i);
    });
    hit.addEventListener("mouseleave", () => {
        cross.setAttribute("visibility", "hidden");
        hideTip();
    });
}

function drawLine(host, rows, valueOf, isBytes) {
    rows = rows.filter(r => valueOf(r) !== null && valueOf(r) !== undefined);
    if (rows.length === 0) {
        host.textContent = "";
        htmlEl("div", "No data in the reports yet.", host, "empty");
        return;
    }
    const fmtY = isBytes ? fmtBytes : fmtInt;
    const times = rows.map(r => r.when.getTime());
    const vals = rows.map(valueOf);
    const f = timeFrame(host, times, Math.max(...vals), isBytes, fmtY);
    const d = times.map((t, i) => (i ? "L" : "M") + f.x(t) + "," + f.y(vals[i])).join("");
    svgEl("path", {d: d, fill: "none", stroke: "var(--series-1)", "stroke-width": 2,
                   "stroke-linejoin": "round", "stroke-linecap": "round"}, f.svg);
    if (rows.length <= 40)
        for (let i = 0; i < rows.length; i++)
            svgEl("circle", {cx: f.x(times[i]), cy: f.y(vals[i]), r: 4,
                             fill: "var(--series-1)", stroke: "var(--surface)",
                             "stroke-width": 2}, f.svg);
    hoverLayer(f, times, (ev, i) => {
        showTip(ev, fmtDate(rows[i].when, true), [{text: fmtY(vals[i])}]);
    });
}

// the biggest formats in the latest report each get a color, the rest fold into Other
function formatSeries() {
    const last = withFormats[withFormats.length - 1];
    const names = Object.keys(last.formats).filter(n => last.formats[n][1] > 0);
    names.sort((a, b) => last.formats[b][1] - last.formats[a][1]);
    const top = names.slice(0, TOP_FORMATS);
    const series = top.map((n, i) => ({name: n, color: SLOTS[i]}));
    series.push({name: "Other", color: OTHER});
    return series;
}

function formatBytes(r, s, top) {
    if (s.name !== "Other")
        return r.formats[s.name] ? r.formats[s.name][1] : 0;
    let sum = 0;
    for (const n in r.formats)
        if (!top.includes(n))
            sum += r.formats[n][1];
    return sum;
}

function drawArea(host, legendHost) {
    host.textContent = "";
    legendHost.textContent = "";
    if (withFormats.length < 2) {
        htmlEl("div", "Needs at least two reports with file sizes.", host, "empty");
        return;
    }
    const series = formatSeries();
    const top = series.map(s => s.name).filter(n => n !== "Other");
    for (const s of series) {
        const item = htmlEl("span", s.name, legendHost);
        item.style.setProperty("--swatch", s.color);
    }
    const times = withFormats.map(r => r.when.getTime());
    const vals = series.map(s => withFormats.map(r => formatBytes(r, s, top)));
    const stacks = [];
    let base = times.map(() => 0);
    for (const v of vals) {
        const upper = base.map((b, i) => b + v[i]);
        stacks.push({lower: base, upper: upper});
        base = upper;
    }
    const f = timeFrame(host, times, Math.max(...base), true, fmtBytes);
    for (let s = 0; s < series.length; s++) {
        const up = times.map((t, i) => f.x(t) + "," + f.y(stacks[s].upper[i]));
        const down = times.map((t, i) => f.x(t) + "," + f.y(stacks[s].lower[i])).reverse();
        svgEl("path", {d: "M" + up.join("L") + "L" + down.join("L") + "Z",
                       fill: series[s].color, stroke: "var(--surface)", "stroke-width": 1.5,
                       "stroke-linejoin": "round"}, f.svg);
    }
    hoverLayer(f, times, (ev, i) => {
        const rows = [];
        for (let s = series.length - 1; s >= 0; s--)
            rows.push({text: series[s].name + "  " + fmtBytes(vals[s][i]),
                       color: series[s].color});
        rows.push({text: "Total  " + fmtBytes(base[i])});
        showTip(ev, fmtDate(withFormats[i].when, true), rows);
    });
}

function drawBars(host) {
    host.textContent = "";
    if (withFormats.length === 0) {
        htmlEl("div", "No report has file sizes yet.", host, "empty");
        return;
    }
    const last = withFormats[withFormats.length - 1];
    document.getElementById("barTitle").textContent = "Report of " + fmtDate(last.when, true);
    const series = formatSeries();
    const colorOf = {};
    for (const s of series)
        colorOf[s.name] = s.color;
    const names = Object.keys(last.formats).filter(n => last.formats[n][1] > 0);
    names.sort((a, b) => last.formats[b][1] - last.formats[a][1]);
    const w = Math.max(host.clientWidth, 260);
    const rowH = 28;
    const m = {l: 120, r: 70};
    const h = names.length * rowH;
    const svg = svgEl("svg", {width: w, height: h, viewBox: "0 0 " + w + " " + h}, host);
    const max = last.formats[names[0]][1];
    const x = v => v / max * (w - m.l - m.r);
    names.forEach((n, i) => {
        const [count, bytes] = last.formats[n];
        const y = i * rowH;
        const g = svgEl("g", {}, svg);
        const label = svgEl("text", {x: m.l - 10, y: y + rowH / 2 + 4, "text-anchor": "end",
                                     fill: "var(--ink-2)", "font-size": 13}, g);
        label.textContent = n.length > 16 ? n.slice(0, 15) + "…" : n;
        svgEl("rect", {x: m.l, y: y + 6, width: Math.max(x(bytes), 2), height: rowH - 12, rx: 3,
                       style: "fill:" + (colorOf[n] || OTHER)}, g);
        const value = svgEl("text", {x: m.l + Math.max(x(bytes), 2) + 6, y: y + rowH / 2 + 4,
                                     fill: "var(--ink-2)", "font-size": 12}, g);
        value.textContent = fmtBytes(bytes);
        const hit = svgEl("rect", {x: 0, y: y, width: w, height: rowH, fill: "transparent"}, g);
        hit.addEventListener("mousemove", ev => {
            showTip(ev, n, [{text: fmtBytes(bytes) + " in " + fmtInt(count) +
                             (count === 1 ? " file" : " files")}]);
        });
        hit.addEventListener("mouseleave", hideTip);
    });
}

function tile(parent, name, value, delta) {
    const card = htmlEl("div", null, parent, "card tile");
    htmlEl("div", name, card, "name");
    htmlEl("div", value, card, "value");
    htmlEl("div", delta, card, "delta");
}

function drawTiles() {
    const host = document.getElementById("tiles");
    const since = previous ? " since " + fmtDate(previous.when, false) : "";
    function delta(key, fmt) {
        if (!previous || latest[key] === null || previous[key] === null)
            return " ";
        return fmtSigned(latest[key] - previous[key], fmt) + since;
    }
    tile(host, "Storage used",
         latest.diskBytes === null ? "—" : fmtBytes(latest.diskBytes),
         delta("diskBytes", fmtBytes));
    tile(host, "Uploaded files", fmtInt(latest.files), delta("files", fmtInt));
    tile(host, "Users with uploads", fmtInt(latest.userCount), delta("userCount", fmtInt));
    let avg = "—";
    if (latest.diskBytes !== null && latest.userCount > 0)
        avg = fmtBytes(latest.diskBytes / latest.userCount);
    tile(host, "Average storage per user", avg, " ");
}

function table(host, head, rows) {
    host.textContent = "";
    const t = htmlEl("table", null, host);
    const tr = htmlEl("tr", null, htmlEl("thead", null, t));
    for (const h of head)
        htmlEl("th", h, tr);
    const body = htmlEl("tbody", null, t);
    for (const r of rows) {
        const row = htmlEl("tr", null, body);
        for (const c of r)
            htmlEl("td", c, row);
    }
}

function drawTables() {
    const dash = "—";
    if (latest.categories)
        table(document.getElementById("tableCategories"), ["Kind", "Files", "Size"],
              latest.categories.map(c => [c[0], fmtInt(c[1]),
                                          c[2] === null ? dash : fmtBytes(c[2])]));
    else
        htmlEl("div", "The latest report does not break files down by kind.",
               document.getElementById("tableCategories"), "empty");
    const num = v => (v === null || v === undefined) ? dash : fmtInt(v);
    table(document.getElementById("tableHealth"), ["Check", "Count"], [
        ["Paths that could not be read", num(latest.unreadable)],
        ["Folders other users cannot open", num(latest.unsearchable)],
        ["Names starting or ending with a space", num(latest.whitespace)],
        ["Files of no known type", num(latest.unknownType)],
    ]);
    const all = reports.slice().reverse().map(r => [
        fmtDate(r.when, true),
        r.diskBytes === null ? dash : fmtBytes(r.diskBytes),
        fmtInt(r.files),
        fmtInt(r.userCount),
        num(r.newUsers),
    ]);
    table(document.getElementById("tableAll"),
          ["Report", "Storage used", "Files", "Users", "New users"], all);
}

function drawCharts() {
    drawLine(document.getElementById("chartDisk"), reports, r => r.diskBytes, true);
    drawLine(document.getElementById("chartFiles"), reports, r => r.files, false);
    drawLine(document.getElementById("chartUsers"), reports, r => r.userCount, false);
    drawArea(document.getElementById("chartArea"), document.getElementById("areaLegend"));
    drawBars(document.getElementById("chartBars"));
}

const first = reports[0];
document.getElementById("subtitle").textContent = DATA.label + " · " + reports.length +
    (reports.length === 1 ? " report" : " reports") + ", " + fmtDate(first.when, true) +
    (reports.length > 1 ? " to " + fmtDate(latest.when, true) : "") +
    " · page made " + DATA.made;
drawTiles();
drawTables();
drawCharts();
let resizeTimer = null;
window.addEventListener("resize", () => {
    clearTimeout(resizeTimer);
    resizeTimer = setTimeout(drawCharts, 150);
});
