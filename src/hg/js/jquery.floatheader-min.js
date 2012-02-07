/* Floating table header plugin
 * floatHeader 1.4.0
 * 1/6/2011
 * http://plugins.jquery.com/project/floatHeader

 * Tested on: FF, Chrome, Opera, IE 6, IE 7/8

 * Usage: jQuery("#table").floatHeader();  
 */

(function (d) {
    function h(c, a, e) {
        c.width(a.width());
        if (!e.forceClass && a.children("thead").length > 0) {
            a = a.children("thead").eq(0).children();
            e = jQuery("<thead/>");
            c.append(e);
            c = e
        } else a = a.find("." + e.markerClass);
        a.each(function () {
            var b = d(this),
                f = b[0].cloneNode(false),
                g = d(f);
            b.children().each(function () {
                var i = d(this),
                    j = i.clone();
                j.width(i.width());
                g.append(j)
            });
            c.append(g)
        })
    }
    function k(c, a, e) {
        c.width(a.width());
        var b;
        if (!e.forceClass && a.children("thead").length > 0) {
            a = a.children("thead").eq(0).children().eq(0);
            b = c.children("thead").eq(0).children().eq(0)
        } else {
            a = a.find("." + e.markerClass).eq(0);
            b = c.children().eq(0)
        }
        b = b.children().eq(0);
        a.children().each(function (f, g) {
            b.width(d(g).width());
            b = b.next()
        })
    }
    function l(c, a) {
        var e = d(c),
            b = d(window).scrollTop(),
            f = e.offset().top,
            g = e.height() - a.height();
        e = e.children("tfoot");
        if (e.length > 0) g -= e.height();
        return f <= b && b <= f + g
    }
    d.fn.floatHeader = function (c) {
        c = d.extend({
            fadeOut: 200,
            fadeIn: 200,
            forceClass: false,
            markerClass: "floating",
            floatClass: "floatHeader",
            recalculate: false,
            IE6Fix_DetectScrollOnBody: true
        }, c);
        return this.each(function () {
            var a = d(this),
                e = a[0].cloneNode(false),
                b = d(e);
            e = b.attr("id") + "FloatHeaderClone";
            b.attr("id", e);
            b.parent().remove();
            a.floatBox = d('<div class="' + c.floatClass + '"style="display:none"></div>');
            a.floatBox.append(b);
            a.IEWindowWidth = document.documentElement.clientWidth;
            a.IEWindowHeight = document.documentElement.clientHeight;
            if (d.browser.msie) {
                if (d.browser.version > 7) c.IE6Fix_DetectScrollOnBody = false
            } else c.IE6Fix_DetectScrollOnBody = false;
            (c.IE6Fix_DetectScrollOnBody ? d("body") : d(window)).scroll(function () {
                if (a.floatBoxVisible) {
                    if (!l(a, a.floatBox)) {
                        var f = a.offset();
                        a.floatBox.css("position", "absolute");
                        a.floatBox.css("top", f.top);
                        a.floatBox.css("left", f.left);
                        a.floatBoxVisible = false;
                        if (c.cbFadeOut) c.cbFadeOut(a.floatBox);
                        else {
                            a.floatBox.stop(true, true);
                            a.floatBox.fadeOut(c.fadeOut)
                        }
                    }
                } else if (l(a, a.floatBox)) {
                    b.children().length === 0 && h(b, a, c);
                    a.floatBoxVisible = true;
                    d.browser.msie && d.browser.version < 7 ? a.floatBox.css("position", "absolute") : a.floatBox.css("position", "fixed");
                    if (c.cbFadeIn) c.cbFadeIn(a.floatBox);
                    else {
                        a.floatBox.stop(true, true);
                        a.floatBox.fadeIn(c.fadeIn)
                    }
                }
                if (a.floatBoxVisible) {
                    d.browser.msie && d.browser.version <= 7 ? a.floatBox.css("top", d(window).scrollTop()) : a.floatBox.css("top", 0);
                    a.floatBox.css("left", a.offset().left - d(window).scrollLeft());
                    c.recalculate && k(b, a, c)
                }
            });
            d.browser.msie && d.browser.version <= 7 ? d(window).resize(function () {
                if (a.IEWindowWidth != document.documentElement.clientWidth || a.IEWindowHeight != document.documentElement.clientHeight) {
                    a.IEWindowWidth = document.documentElement.clientWidth;
                    a.IEWindowHeight = document.documentElement.clientHeight;
                    if (b.children().length > 0) {
                        b.fastempty();
                        h(b, a, c)
                    }
                }
            }) : d(window).resize(function () {
                if (b.children().length > 0) {
                    b.fastempty();
                    h(b, a, c)
                }
            });
            d(a).after(a.floatBox);
            this.fhRecalculate = function () {
                k(b, a, c)
            };
            this.fhInit = function () {
                if (b.children().length > 0) {
                    b.fastempty();
                    h(b, a, c)
                }
            };
            d.fn.fastempty = function () {
                if (this[0]) for (; this[0].hasChildNodes();) this[0].removeChild(this[0].lastChild);
                return this
            }
        })
    }
})(jQuery);
