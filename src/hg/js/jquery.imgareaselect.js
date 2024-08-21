/*
 * imgAreaSelect jQuery plugin
 * version 0.6.2
 *
 * Copyright (c) 2008 Michal Wojciechowski (odyniec.net)
 *
 * Dual licensed under the MIT (MIT-LICENSE.txt)
 * and GPL (GPL-LICENSE.txt) licenses.
 *
 * http://odyniec.net/projects/imgareaselect/
 *
 * modified by larrym to support hgTracks functionality; added:
 *
 * o options.clickClipHeight - allows click through to map items
 *
 * o selection.event - provides access mouse event object to callbacks
 *
 * modified by max to support: minX, maxX - do not allow selecting before or after a given x position
 *
 */

jQuery.imgAreaSelect = { onKeyPress: null };

jQuery.imgAreaSelect.init = function (img, options) {
    var $area = jQuery('<div></div>'),
        $border1 = jQuery('<div></div>'),
        $border2 = jQuery('<div></div>'),
        $outLeft = jQuery('<div></div>'),
        $outTop = jQuery('<div></div>'),
        $outRight = jQuery('<div></div>'),
        $outBottom = jQuery('<div></div>'),
        left, top, imgOfs, imgWidth, imgHeight, parent, parOfs, parScroll,
        adjusted, zIndex = 0, fixed, $p, startX, startY, moveX, moveY,
        resizeMargin = 10, resize = [ ], V = 0, H = 1,
        keyDown, d, aspectRatio, x1, x2, y1, y2, x, y,
        selection = { x1: 0, y1: 0, x2: 0, y2: 0, width: 0, height: 0, event: null};

    var $a = $area.add($border1).add($border2);
    var $o = $outLeft.add($outTop).add($outRight).add($outBottom);

    function viewX(x)
    {
        return x + imgOfs.left + parScroll.left - parOfs.left;
    }

    function viewY(y)
    {
        return y + imgOfs.top + parScroll.top - parOfs.top;
    }

    function selX(x)
    {
        return x - imgOfs.left - parScroll.left + parOfs.left;
    }

    function selY(y)
    {
        return y - imgOfs.top - parScroll.top + parOfs.top;
    }

    function evX(event)
    {
        return event.pageX + parScroll.left - parOfs.left;
    }

    function evY(event)
    {
        return event.pageY + parScroll.top - parOfs.top;
    }

    function adjust()
    {
        imgOfs = jQuery(img).offset();
        imgWidth = jQuery(img).width();
        imgHeight = jQuery(img).height();

        if (jQuery(parent).is('body'))
            parOfs = parScroll = { left: 0, top: 0 };
        else {
            parOfs = jQuery(parent).offset();
            parScroll = { left: parent.scrollLeft, top: parent.scrollTop };
        }

        left = viewX(0);
        top = viewY(0);
    }

    function update(resetKeyPress)
    {
        $a.css({
            left: viewX(selection.x1) + 'px',
            top: viewY(selection.y1) + 'px',
            width: Math.max(selection.width - options.borderWidth * 2, 0) + 'px',
            height: Math.max(selection.height - options.borderWidth * 2, 0) + 'px'
        });
        $outLeft.css({ left: left + 'px', top: top + 'px',
            width: selection.x1 + 'px', height: imgHeight + 'px' });
        $outTop.css({ left: left + selection.x1 + 'px', top: top + 'px',
            width: selection.width + 'px', height: selection.y1 + 'px' });
        $outRight.css({ left: left + selection.x2 + 'px', top: top + 'px',
            width: imgWidth - selection.x2 + 'px', height: imgHeight + 'px' });
        $outBottom.css({ left: left + selection.x1 + 'px', top: top + selection.y2 + 'px',
            width: selection.width + 'px', height: imgHeight - selection.y2 + 'px' });

        if (resetKeyPress !== false) {
            if (jQuery.imgAreaSelect.keyPress != docKeyPress)
                jQuery(document).unbind(jQuery.imgAreaSelect.keyPress,
                    jQuery.imgAreaSelect.onKeyPress);

            if (options.keys)
                jQuery(document).bind(jQuery.imgAreaSelect.keyPress,
                    jQuery.imgAreaSelect.onKeyPress = docKeyPress);
        }
    }

    function areaMouseMove(event)
    {
        if (!adjusted) {
            adjust();
            adjusted = true;

            $a.one('mouseout', function () { adjusted = false; });
        }

        x = selX(evX(event)) - selection.x1;
        y = selY(evY(event)) - selection.y1;

        resize = [ ];

        if (options.resizable) {
            if (y <= resizeMargin)
                resize[V] = 'n';
            else if (y >= selection.height - resizeMargin)
                resize[V] = 's';
            if (x <= resizeMargin)
                resize[H] = 'w';
            else if (x >= selection.width - resizeMargin)
                resize[H] = 'e';
        }

        $border2.css('cursor', resize.length ? resize.join('') + '-resize' :
            options.movable ? 'move' : '');
    }

    function areaMouseDown(event)
    {
        if (event.which > 1 || event.button > 1) return true;
        selection.event = event;

        adjust();

        if (options.resizable && resize.length > 0) {
            jQuery('body').css('cursor', resize.join('') + '-resize');

            x1 = viewX(resize[H] == 'w' ? selection.x2 : selection.x1);
            y1 = viewY(resize[V] == 'n' ? selection.y2 : selection.y1);

            jQuery(document).mousemove(selectingMouseMove);
            $border2.unbind('mousemove', areaMouseMove);

            jQuery(document).one('mouseup', function (event) {
                selection.event = event;
                resize = [ ];

                jQuery('body').css('cursor', '');

                if (options.autoHide)
                    $a.add($o).hide();

                options.onSelectEnd(img, selection, event);

                jQuery(document).unbind('mousemove', selectingMouseMove);
                $border2.mousemove(areaMouseMove);
            });
        }
        else if (options.movable) {
            moveX = selection.x1 + left;
            moveY = selection.y1 + top;
            startX = evX(event);
            startY = evY(event);

            jQuery(document).mousemove(movingMouseMove)
                .one('mouseup', function (event) {
                    selection.event = event;
                    options.onSelectEnd(img, selection, event);

                    jQuery(document).unbind('mousemove', movingMouseMove);
                });
        }
        else
            jQuery(img).mousedown(event);

        return false;
    }

    function aspectRatioXY()
    {
        x2 = Math.max(left, Math.min(left + imgWidth,
            x1 + Math.abs(y2 - y1) * aspectRatio * (x2 >= x1 ? 1 : -1)));
        y2 = Math.round(Math.max(top, Math.min(top + imgHeight,
            y1 + Math.abs(x2 - x1) / aspectRatio * (y2 >= y1 ? 1 : -1))));
        x2 = Math.round(x2);
    }

    function aspectRatioYX()
    {
        y2 = Math.max(top, Math.min(top + imgHeight,
            y1 + Math.abs(x2 - x1) / aspectRatio * (y2 >= y1 ? 1 : -1)));
        x2 = Math.round(Math.max(left, Math.min(left + imgWidth,
            x1 + Math.abs(y2 - y1) * aspectRatio * (x2 >= x1 ? 1 : -1))));
        y2 = Math.round(y2);
    }

    function doResize(newX2, newY2)
    {
        x2 = newX2;
        y2 = newY2;

        if (options.minX && x2 < options.minX)
            x2 = options.minX;
        if (options.maxX && x2 > options.maxX)
            x2 = options.maxX;

        if (options.minWidth && Math.abs(x2 - x1) < options.minWidth) {
            x2 = x1 - options.minWidth * (x2 < x1 ? 1 : -1);

            if (x2 < left)
                x1 = left + options.minWidth;

            else if (x2 > left + imgWidth)
                x1 = left + imgWidth - options.minWidth;
        }

        if (options.minHeight && Math.abs(y2 - y1) < options.minHeight) {
            y2 = y1 - options.minHeight * (y2 < y1 ? 1 : -1);

            if (y2 < top)
                y1 = top + options.minHeight;
            else if (y2 > top + imgHeight)
                y1 = top + imgHeight - options.minHeight;
        }

        x2 = Math.max(left, Math.min(x2, left + imgWidth));
        y2 = Math.max(top, Math.min(y2, top + imgHeight));

        if (aspectRatio)
            if (Math.abs(x2 - x1) / aspectRatio > Math.abs(y2 - y1))
                aspectRatioYX();
            else
                aspectRatioXY();

        if (options.maxWidth && Math.abs(x2 - x1) > options.maxWidth) {
            x2 = x1 - options.maxWidth * (x2 < x1 ? 1 : -1);
            if (aspectRatio) aspectRatioYX();
        }

        if (options.maxHeight && Math.abs(y2 - y1) > options.maxHeight) {
            y2 = y1 - options.maxHeight * (y2 < y1 ? 1 : -1);
            if (aspectRatio) aspectRatioXY();
        }

        selection.x1 = selX(Math.min(x1, x2));
        selection.x2 = selX(Math.max(x1, x2));
        selection.y1 = selY(Math.min(y1, y2));
        selection.y2 = selY(Math.max(y1, y2));
        selection.width = Math.abs(x2 - x1);
        selection.height = Math.abs(y2 - y1);

        update();

        options.onSelectChange(img, selection);
    }

    function selectingMouseMove(event)
    {
        selection.event = event;
        x2 = !resize.length || resize[H] || aspectRatio ? evX(event) : viewX(selection.x2);
        y2 = !resize.length || resize[V] || aspectRatio ? evY(event) : viewY(selection.y2);

        doResize(x2, y2);

        return false;
    }

    function doMove(newX1, newY1)
    {
        x2 = (x1 = newX1) + selection.width;
        y2 = (y1 = newY1) + selection.height;

        selection.x1 = selX(x1);
        selection.y1 = selY(y1);
        selection.x2 = selX(x2);
        selection.y2 = selY(y2);

        update();

        options.onSelectChange(img, selection);
    }

    function movingMouseMove(event)
    {
        selection.event = event;
        var newX1 = Math.max(left, Math.min(moveX + evX(event) - startX,
            left + imgWidth - selection.width));
        var newY1 = Math.max(top, Math.min(moveY + evY(event) - startY,
            top + imgHeight - selection.height));

        doMove(newX1, newY1);

        event.preventDefault();
        return false;
    }

    function startSelection(event)
    {
        adjust();

        selection.x1 = selection.x2 = selX(startX = x1 = x2 = evX(event));
        selection.y1 = selection.y2 = selY(startY = y1 = y2 = evY(event));
        selection.width = 0;
        selection.height = 0;
        selection.event = event;

        if(options.clickClipHeight != null) {
            // This is necessary on IE to support clicks in an image which has map items.
            var found = false;
            if(typeof(options.clickClipHeight) == 'number') {
                if(selection.y1 < options.clickClipHeight) {
                    found = true;
                }
            } else {
                for(var i = 0; !found && i < options.clickClipHeight.length; i++) {
                    if(selection.y1 >= options.clickClipHeight[i].top && selection.y1 < options.clickClipHeight[i].bottom) {
                        found = true;
                    }
                }
            }
            found = found || event.shiftKey || event.metaKey || event.altKey || event.ctrlKey;
            if(!found) {
                return false;
            }
        }

        resize = [ ];

        update();
        $a.add($o).show();
        $o.css('cursor', "crosshair"); // Outside of borders (needed for dragging right)
        $a.css('cursor', "crosshair"); // Inside borders (needed for dragging left)

        jQuery(document).unbind('mouseup', cancelSelection)
            .mousemove(selectingMouseMove);
        $border2.unbind('mousemove', areaMouseMove);

        options.onSelectStart(img, selection);

        jQuery(document).one('mouseup', function (event) {
            selection.event = event;
            if (options.autoHide || (selection.width * selection.height == 0))
                $a.add($o).hide();

            options.onSelectEnd(img, selection, event);

            //$o.css('cursor', ""); // Not even necessary
            //$a.css('cursor', "");

            jQuery(document).unbind('mousemove', selectingMouseMove);
            $border2.mousemove(areaMouseMove);
        });
    }

    function cancelSelection(event)
    {
        // The default behavior of imgareaselect is to cancel selection if the user let's go of the mouse before
        // moving it; we change the behavior so we can treat this as a single click, centering event.
        selection.event = event;
        jQuery(document).unbind('mousemove', startSelection);
        $a.add($o).hide();

        selection.x1 = selection.x2 = selX(startX = x1 = x2 = evX(event));
        selection.y1 = selection.y2 = selY(startY = y1 = y2 = evY(event));

        options.onSelectChange(img, selection);
        options.onSelectEnd(img, selection, event);
    }

    function imgMouseDown(event)
    {
        if (event.which > 1 || event.button > 1) return true;

        jQuery(document).one('mousemove', startSelection)
            .one('mouseup', cancelSelection);

        return false;
    }

    function windowResize()
    {
        adjust();
        update(false);
        x1 = viewX(selection.x1);
        y1 = viewY(selection.y1);
        x2 = viewX(selection.x2);
        y2 = viewY(selection.y2);
    }

    var docKeyPress = function(event) {
        var k = options.keys, d = 10, t,
            key = event.keyCode || event.which;

        if (!isNaN(k.arrows)) d = k.arrows;
        if (!isNaN(k.shift) && event.shiftKey) d = k.shift;
        if (!isNaN(k.ctrl) && event.ctrlKey) d = k.ctrl;
        if (!isNaN(k.alt) && (event.altKey || event.originalEvent.altKey)) d = k.alt;

        if (k.arrows == 'resize' || (k.shift == 'resize' && event.shiftKey) ||
            (k.ctrl == 'resize' && event.ctrlKey) ||
            (k.alt == 'resize' && (event.altKey || event.originalEvent.altKey)))
        {
            switch (key) {
            case 37:
                d = -d;
            case 39:
                t = Math.max(x1, x2);
                x1 = Math.min(x1, x2);
                x2 = Math.max(t + d, x1);
                if (aspectRatio) aspectRatioYX();
                break;
            case 38:
                d = -d;
            case 40:
                t = Math.max(y1, y2);
                y1 = Math.min(y1, y2);
                y2 = Math.max(t + d, y1);
                if (aspectRatio) aspectRatioXY();
                break;
            default:
                return;
            }

            doResize(x2, y2);
        }
        else {
            x1 = Math.min(x1, x2);
            y1 = Math.min(y1, y2);

            switch (key) {
            case 37:
                doMove(Math.max(x1 - d, left), y1);
                break;
            case 38:
                doMove(x1, Math.max(y1 - d, top));
                break;
            case 39:
                doMove(x1 + Math.min(d, imgWidth - selX(x2)), y1);
                break;
            case 40:
                doMove(x1, y1 + Math.min(d, imgHeight - selY(y2)));
                break;
            default:
                return;
            }
        }

        return false;
    };

    this.setOptions = function(newOptions)
    {
        options = jQuery.extend(options, newOptions);

        if (newOptions.x1 != null) {
            selection.x1 = newOptions.x1;
            selection.y1 = newOptions.y1;
            selection.x2 = newOptions.x2;
            selection.y2 = newOptions.y2;
            newOptions.show = true;
        }

        if (newOptions.keys)
            options.keys = jQuery.extend({ shift: 1, ctrl: 'resize' },
                newOptions.keys === true ? { } : newOptions.keys);

        parent = jQuery(options.parent).get(0);

        adjust();

        $p = jQuery(img);

        while ($p.length && !$p.is('body')) {
            if (!isNaN($p.css('z-index')) && $p.css('z-index') > zIndex)
                zIndex = $p.css('z-index');
            if ($p.css('position') == 'fixed') fixed = true;

            $p = $p.parent();
        }

        x1 = viewX(selection.x1);
        y1 = viewY(selection.y1);
        x2 = viewX(selection.x2);
        y2 = viewY(selection.y2);
        selection.width = x2 - x1;
        selection.height = y2 - y1;

        update();

        if (newOptions.hide)
            $a.add($o).hide();
        else if (newOptions.show)
            $a.add($o).show();

        $o.addClass(options.classPrefix + '-outer');
        $area.addClass(options.classPrefix + '-selection');
        $border1.addClass(options.classPrefix + '-border1');
        $border2.addClass(options.classPrefix + '-border2');

        $a.css({ borderWidth: options.borderWidth + 'px' });
        $area.css({ backgroundColor: options.selectionColor, opacity: options.selectionOpacity });
        $border1.css({ borderStyle: 'solid', borderColor: options.borderColor1 });
        $border2.css({ borderStyle: 'dashed', borderColor: options.borderColor2 });
        $o.css({ opacity: options.outerOpacity, backgroundColor: options.outerColor });

        aspectRatio = options.aspectRatio && (d = options.aspectRatio.split(/:/)) ?
            d[0] / d[1] : null;

        if (options.disable || options.enable === false) {
            $a.unbind('mousemove', areaMouseMove).unbind('mousedown', areaMouseDown);
            jQuery(img).add($o).unbind('mousedown', imgMouseDown);
            jQuery(window).unbind('resize', windowResize);
        }
        else if (options.enable || options.disable === false) {
            if (options.resizable || options.movable)
                $a.mousemove(areaMouseMove).mousedown(areaMouseDown);

            if (!options.persistent)
                jQuery(img).add($o).mousedown(imgMouseDown);
            jQuery(window).resize(windowResize);
        }

        jQuery(options.parent).append($o.add($a));

        options.enable = options.disable = undefined;
    };

    jQuery.imgAreaSelect.keyPress = 'keypress';
    

    $a.add($o).css({ display: 'none', position: fixed ? 'fixed' : 'absolute',
        overflow: 'hidden', zIndex: zIndex > 0 ? zIndex : '0' });
    $area.css({ borderStyle: 'solid' });

    initOptions = {
        borderColor1: '#000',
        borderColor2: '#fff',
        borderWidth: 1,
        classPrefix: 'imgareaselect',
        movable: true,
        resizable: true,
        selectionColor: '#fff',
        selectionOpacity: 0.2,
        outerColor: '#000',
        outerOpacity: 0.2,
        parent: 'body',
        onSelectStart: function () {},
        onSelectChange: function () {},
        onSelectEnd: function () {}
    };

    options = jQuery.extend(initOptions, options);
    this.setOptions(options);
};

jQuery.fn.imgAreaSelect = function (options) {
    options = options || {};

    this.each(function () {
        if (jQuery(this).data('imgAreaSelect'))
            jQuery(this).data('imgAreaSelect').setOptions(options);
        else {
            if (options.enable === undefined && options.disable === undefined)
                options.enable = true;

            jQuery(this).data('imgAreaSelect', new jQuery.imgAreaSelect.init(this, options));
        }
    });

    return this;
};
