// Module: gtexTrackSettings


var gtexTrackSettings = (function() {

    // Data
    
    // SVG has its own DOM
    var _svgDoc;
    var _svgRoot;
    var _htmlDoc;
    var _htmlRoot;
    var _topTissue;
    var _topAura;

    var tissues = [
        'adiposeSubcut', 'adiposeVisceral', 'adrenalGland', 'arteryAorta', 'arteryCoronary', 
        'arteryTibial', 'bladder', 'brainAmygdala', 'brainAnCinCortex', 'brainCaudate', 
        'brainCerebelHemi', 'brainCerebellum', 'brainCortex', 'brainFrontCortex', 
        'brainHippocampus', 'brainHypothalamus', 'brainNucAccumbens', 'brainPutamen', 
        'brainSpinalcord', 'brainSubstanNigra', 'breastMamTissue', 'xformedlymphocytes',
        'xformedfibroblasts', 'ectocervix', 'endocervix', 'colonSigmoid', 'colonTransverse',
        'esophagusJunction', 'esophagusMucosa', 'esophagusMuscular', 'fallopianTube', 
        'heartAtrialAppend', 'heartLeftVentricl', 'kidneyCortex', 'liver', 'lung', 
        'minorSalivGland', 'muscleSkeletal', 'nerveTibial', 'ovary', 'pancreas', 'pituitary', 
        'prostate', 'skinNotExposed', 'skinExposed', 'smallIntestine', 'spleen', 'stomach', 
        'testis', 'thyroid', 'uterus', 'vagina', 'wholeBlood'
    ];

    // Convenience functions
    function tissueFromSvgId(svgId) {
    // Get tissue name from an SVG id. Convention here is <tis>_*
            return svgId.split('_')[0];
    }

    function initTissue(tis) {
        // Set tissue to unhighlighted state
        $("#" + tis + "_Pic_Hi", _svgRoot).hide();

        // $("#" + tis + "_Lead_Hi", _svgRoot).hide();

        // mark tissue labels in svg
        var el = _svgDoc.getElementById(tis + "_Text_Hi");
        if (el !== null) {
            el.classList.add('tissueLabel');
        }
        $("#" + tis + "_Aura_Hi", _svgRoot).hide();

        // force tissue to selected on init (till we have cart hooked up)
        //onClickSetTissue(tis);
    }

    function highlightTissue(tis) {
        // 
    }

    function initBodyMap(svg, doc) {
        // Set organs to unhighlighted state
        tissues.forEach(initTissue);
    }


    function onClickToggleTissue(tis) {
        // mark selected in tissue table
        tis = this.id;  // arg bad
        $(this).toggleClass('tissueSelected');

        // jQuery addClass doesn't work on SVG elements, using classList
        // May need a shim so this works on IE9 and Opera mini (as of Dec 2014)
        // https://martinwolf.org/blog/2014/12/adding-and-removing-classes-from-svg-elements-with-jquery
        var el = _svgDoc.getElementById(this.id + '_Text_Hi');
        if (el !== null) {
            el.classList.toggle('tissueSelected');
            if ($(this).hasClass('tissueSelected')) {
                onClickSetTissue(tis);
            } else {
                onClickClearTissue(tis);
            }
        }
    }

    function onMapClickToggleTissue(ev) {
        var svgId = ev.target.id;
        var el = _svgDoc.getElementById(svgId);
        if (el !== null) {
            el.classList.toggle('tissueSelected');
        }
        var tis = tissueFromSvgId(svgId);
        el = _htmlDoc.getElementById(tis);
        if (el !== null) {
            el.classList.toggle('tissueSelected');
            if (el.classList.contains('tissueSelected')) {
                onClickSetTissue(tis);
            } else {
               onClickClearTissue(tis);
            }
        }
    }

    function setMapTissueEl(el) {
        // set label dark
        if (el !== null) {
            el.classList.add('tissueSelected');
            el.style.fill = "black";
            var count = el.childElementCount;
            for (var i = 0; i < count; i++) {
                el.children[i].style.fill = "black";
            }
        }
    }

    function onClickSetTissue(tis) {
        // mark selected in tissue table
        var $tis = $('#' + tis);
        $tis.addClass('tissueSelected');
        var colorPatch = $tis.prev(".tissueColor");
        colorPatch.removeClass('tissueNotSelectedColor');
        var tisColor = colorPatch.data('tissueColor');
        colorPatch.css('background-color', tisColor);
        var $checkbox = $('#' + tis + ' > input');
        $checkbox.attr("checked", true);
        var el = _svgDoc.getElementById(tis + "_Text_Hi");
        setMapTissueEl(el);
    }

    function onClickClearTissue(tis) {
        // unselect in tissue table
        var $tis = $('#' + tis);
        $tis.removeClass('tissueSelected');
        colorPatch = $tis.prev(".tissueColor");
        colorPatch.addClass('tissueNotSelectedColor');
        var $checkbox = $('#' + tis + ' > input');
        $checkbox.attr("checked", false);
        var el = _svgDoc.getElementById(tis + "_Text_Hi");
        if (el !== null) {
            el.classList.remove('tissueSelected');
            el.style.fill = "#737373";
            var count = el.childElementCount;
            for (var i = 0; i < count; i++) {
                el.children[i].style.fill = "#737373";
            }
        }
    }


    function toggleHighlightTissue(tis) {
        var i;
        var isHovered = false;
        var el = _htmlDoc.getElementById(tis);
        var $tis = $("#" + tis);
        if (el !== null) {
            el.classList.toggle('tissueHovered');
            var colorPatch = $tis.prev(".tissueColor");
            colorPatch.toggleClass('tissueHoveredColor');
            if (el.classList.contains('tissueHovered')) {
                isHovered = true;
            }
        }
        // below can likely replace 3 lines after
        //this.classList.toggle('tissueSelected');
        textEl = _svgDoc.getElementById(tis + '_Text_Hi');
        if (textEl === null) {
            return;
        }
        textEl.classList.toggle('tissueHovered');
        var line = $("#" + tis + "_Lead_Hi", _svgRoot);
        var lineEl = _svgDoc.getElementById(tis + '_Lead_Hi');
        var pic = $("#" + tis + "_Pic_Hi", _svgRoot);
        var picEl = _svgDoc.getElementById(tis + '_Pic_Hi');
        var aura = $("#" + tis + "_Aura_Hi", _svgRoot);
        var auraEl = _svgDoc.getElementById(tis + '_Aura_Hi');
        var textLineCount = textEl.childElementCount;
        if (isHovered) {
            textEl.style.fill = 'blue';
            for (i = 0; i < textLineCount; i++) {
                textEl.children[i].style.fill = "blue";
            }
            //$(line).show();
            //lineEl.style.stroke = '#EC1C24';
            //lineEl.style.stroke = 'red';
            if (lineEl !== null) {     // cell types lack leader lines
            lineEl.style.stroke = 'blue';
            }
            //lineEl.setAttribute("style", "stroke-width: 3; stroke: red;");
            //
            $(pic).show();
            $(aura).show();

            var topAura = auraEl.cloneNode(true);
            topAura.id = "topAura";
            _topAura = _svgRoot.appendChild(topAura);
        
            var topTissue = picEl.cloneNode(true);
            topTissue.id = "topTissue";
            _topTissue = _svgRoot.appendChild(topTissue);
        } else {
            var color = textEl.classList.contains('tissueSelected') ? 'black' : '#737373';
            textEl.style.fill = color;
            for (i = 0; i < textLineCount; i++) {
                textEl.children[i].style.fill = color;
            }
            $(aura).hide();
            $(pic).hide();
            //$(line).hide();
            //lineEl.style.stroke = '#EC1C24';      // red
            if (lineEl !== null) {     // cell types lack leader lines
                lineEl.style.stroke = '#F69296';      // pink
            }
            //lineEl.setAttribute("style", "stroke-width: 1.7; stroke: #F69296;");
            //
            _svgRoot.removeChild(_topTissue);
            _svgRoot.removeChild(_topAura);
        }
    }

    function onMapHoverTissue(ev) {
        var svgId = ev.target.id;
        var tis = tissueFromSvgId(svgId);
        toggleHighlightTissue(tis);
    }

    function onHoverTissue() {
        var tis = this.id;
        toggleHighlightTissue(tis);
    }

    //function animateTissue(tis, i, ignore) {
    function animateTissue(tis) {
        //console.log(tis);
        // add handlers to tissue table
        var textEl;
        var picEl;
        $('#' + tis).click(tis, onClickToggleTissue);
        $('#' + tis).hover(onHoverTissue, onHoverTissue);

        // add mouseover and click handlers to tissue label
        textEl = _svgDoc.getElementById(tis + "_Text_Hi");
        if (textEl !== null) {
            if ($('#' + tis).hasClass('tissueSelected')) {
                setMapTissueEl(textEl);
            }
            textEl.addEventListener("click", onMapClickToggleTissue);
            textEl.addEventListener("mouseenter", onMapHoverTissue);
            textEl.addEventListener("mouseleave", onMapHoverTissue);
            // mouseover, mouseout ?
        }
        // add mouseover and click handlers to tissue shape
        picEl = _svgDoc.getElementById(tis + "_Pic_Lo");
        if (picEl !== null) {
            picEl.addEventListener("click", onMapClickToggleTissue);
            picEl.addEventListener("mouseenter", onMapHoverTissue);
            picEl.addEventListener("mouseleave", onMapHoverTissue);
            // mouseover, mouseout ?
        }
    }

    function animateTissues() {
        // Add event handlers to tissue table and body map SVG
        tissues.forEach(animateTissue);

        $('#setAll').click(onClickSetAll);
        $('#clearAll').click(onClickClearAll);
    }

    // UI event handlers

    function onClickSetAll() {
        // set all on body map
        tissues.forEach(onClickSetTissue);
        // set all on tissue table
        // NOTE: this shouldn't be needed (needs debugging in onClickSet)
        $('.tissueLabel').addClass('tissueSelected');
    }

    function onClickClearAll() {
        tissues.forEach(onClickClearTissue);
        $('.tissueLabel').removeClass("tissueSelected");
    }

    function onClickMapTissue() {
        this.style.fill = "black";
        //document.getElementById("bodyMapSvg").contentDocument.getElementById('endocervix').style.fill = "black";
    }

    function submitForm() {
    // Submit form from go button (see hgGateway.js)
    // Show a spinner -- sometimes it takes a while for hgTracks to start displaying.
    $('.jwGoIcon').removeClass('fa-play').addClass('fa-spinner fa-spin');
    $form = $('form');
    $form.submit();
    }

    // Initialization

    function init() {
        // cart.setCgi('gtexTrackSettings');

        $(function() {
            // after page load, tweak layout and initialize event handlers
            var bodyMapSvg = document.getElementById("bodyMapSvg");
            globalSvg = bodyMapSvg;

            _htmlDoc = document;
            _htmlRoot = document.documentElement;

            // wait for SVG to load
            bodyMapSvg.addEventListener("load", function() {
                var svg = bodyMapSvg.contentDocument.documentElement;
                _svgRoot = svg;
                _svgDoc = bodyMapSvg.contentDocument;
                
                initBodyMap(svg, bodyMapSvg.contentDocument);
                animateTissues();
            }, false);

            $('.goButtonContainer').click(submitForm);
        });
    }

    return {
            init: init
           };
    
}()); // gtexTrackSettings

gtexTrackSettings.init();
