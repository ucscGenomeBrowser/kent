// Module: gtexTrackSettings


var gtexTrackSettings = (function() {

    // Data
    
    // SVG has its own DOM
    var _svgDoc;
    var _svgRoot;
    var _htmlDoc;
    var _htmlRoot;

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
        $("#" + tis + "_Lead_Hi", _svgRoot).hide();

        // mark tissue labels in svg
        var el = _svgDoc.getElementById(tis + "_Text_Hi");
        if (el !== null) {
            el.classList.add('tissueLabel');
        }
        $("#" + tis + "_Aura_Hi", _svgRoot).hide();
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
        $(this).toggleClass('tissueSelected');

        // jQuery addClass doesn't work on SVG elements, using classList
        // May need a shim so this works on IE9 and Opera mini (as if Dec 2014)
        // https://martinwolf.org/blog/2014/12/adding-and-removing-classes-from-svg-elements-with-jquery
        var el = _svgDoc.getElementById(this.id + '_Text_Hi');
        if (el !== null) {
            el.classList.toggle('tissueSelected');
        }
    }

    function onMapClickToggleTissue(ev) {
        var isOn = false;
        var svgId = ev.target.id;
        var tis = tissueFromSvgId(svgId);
        var el = _htmlDoc.getElementById(tis);
        if (el !== null) {
            el.classList.toggle('tissueSelected');
        }
        if (el.classList.contains('tissueSelected')) {
            isOn = true;
        }
        // below can likely replace 3 lines after
        //this.classList.toggle('tissueSelected');
        el = _svgDoc.getElementById(svgId);
        if (el !== null) {
            el.classList.toggle('tissueSelected');
            if (isOn) {
                el.style.fill = "black";
            } else {
                el.style.fill = "#737373";
            }
        }
    }

    function onClickSetTissue(tis) {
        // mark selected in tissue table
        $(tis).addClass('tissueSelected');
        var el = _svgDoc.getElementById(tis + "_Text_Hi");
        if (el !== null) {
            el.classList.add('tissueSelected');
            el.style.fill = "black";
            var count = el.childElementCount;
            for (var i = 0; i < count; i++) {
                el.children[i].style.fill = "black";
            }
        }
    }

    function onClickClearTissue(tis) {
        // mark selected in tissue table
        $(tis).removeClass('tissueSelected');
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


    function onMapHoverTissue(ev) {
        var i;
        var isOn = false;
        var svgId = ev.target.id;
        var tis = tissueFromSvgId(svgId);
        var el = _htmlDoc.getElementById(tis);
        if (el !== null) {
            el.classList.toggle('tissueHovered');
            if (el.classList.contains('tissueHovered')) {
                isOn = true;
            }
        }
        // below can likely replace 3 lines after
        //this.classList.toggle('tissueSelected');
        el = _svgDoc.getElementById(tis + '_Text_Hi');
        if (el === null)
            return;
        el.classList.toggle('tissueHovered');
        var line = $("#" + tis + "_Lead_Hi", _svgRoot);
        var pic = $("#" + tis + "_Pic_Hi", _svgRoot);
        var white = $("#" + tis + "_Aura_Hi", _svgRoot);
        var count = el.childElementCount;
        if (isOn) {
            el.style.fill = 'blue';
            for (i = 0; i < count; i++) {
                el.children[i].style.fill = "blue";
            }
            $(line).show();
            $(pic).show();
            $(white).show();
        } else {
            var color = el.classList.contains('tissueSelected') ? 'black' : '#737373';
            el.style.fill = color;
            for (i = 0; i < count; i++) {
                el.children[i].style.fill = color;
            }
            $(white).hide();
            $(pic).hide();
            $(line).hide();
        }
    }

    function onHoverTissue() {
        // HTML
        $(this).toggleClass('tissueHovered');
        var isOn = $(this).hasClass('tissueHovered');

        // SVG
        var el = _svgDoc.getElementById(this.id + '_Text_Hi');
        if (el !== null) {
            el.classList.toggle('tissueHovered');
            if (this.id === "arteryAorta") {
                var line = $("#LL_arteryAorta", _svgRoot);
                var white = $("#WHITE_arteryAorta", _svgRoot);
                if (isOn) {
                    $(line).show();
                    $(white).show();
                } else {
                    $(white).hide();
                    $(line).hide();
                }
/*
                el = _svgDoc.getElementById("LL_arteryAorta");
                if (el !== null) {
                    el.classList.toggle('mapTissueHovered');
                }
                el = _svgDoc.getElementById("WHITE_arteryAorta");
                if (el !== null) {
                    el.classList.toggle('mapTissueHovered');
*/
            }
        }
    }

    //function animateTissue(tis, i, ignore) {
    function animateTissue(tis) {
        //console.log(tis);
        // add handlers to tissue table
        $('#' + tis).click(tis, onClickToggleTissue);
        $('#' + tis).hover(onHoverTissue, onHoverTissue);

        var el = _svgDoc.getElementById(tis + "_Text_Hi");
        if (el !== null) {
            el.addEventListener("click", onMapClickToggleTissue);
            el.addEventListener("mouseenter", onMapHoverTissue);
            el.addEventListener("mouseleave", onMapHoverTissue);
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

        });
    }

    return {
            init: init
           };
    
}()); // gtexTrackSettings

gtexTrackSettings.init();
