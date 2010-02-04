/**
 * Extending jQuery with autocomplete
 * Version: 1.4.2
 * Author: Yanik Gleyzer (clonyara)
 */
(function($) {

// some key codes
 var RETURN = 13;
 var TAB = 9;
 var ESC = 27;
 var ARRLEFT = 37;
 var ARRUP = 38;
 var ARRRIGHT = 39;
 var ARRDOWN = 40;
 var BACKSPACE = 8;
 var DELETE = 46;
 
function debug(s){
  $('#info').append(htmlspecialchars(s)+'<br>');
}
// getting caret position obj: {start,end}
function getCaretPosition(obj){
  var start = -1;
  var end = -1;
  if(typeof obj.selectionStart != "undefined"){
    start = obj.selectionStart;
    end = obj.selectionEnd;
  }
  else if(document.selection&&document.selection.createRange){
    var M=document.selection.createRange();
    var Lp;
    try{
      Lp = M.duplicate();
      Lp.moveToElementText(obj);
    }catch(e){
      Lp=obj.createTextRange();
    }
    Lp.setEndPoint("EndToStart",M);
    start=Lp.text.length;
    if(start>obj.value.length)
      start = -1;
    
    Lp.setEndPoint("EndToStart",M);
    end=Lp.text.length;
    if(end>obj.value.length)
      end = -1;
  }
  return {'start':start,'end':end};
}
// set caret to
function setCaret(obj,l){
  obj.focus();
  if (obj.setSelectionRange){
    obj.setSelectionRange(l,l);
  }
  else if(obj.createTextRange){
    m = obj.createTextRange();      
    m.moveStart('character',l);
    m.collapse();
    m.select();
  }
}
// prepare array with velued objects
// required properties are id and value
// rest of properties remaines
function prepareArray(jsondata){
  var new_arr = [];
  for(var i=0;i<jsondata.length;i++){
    if(jsondata[i].id != undefined && jsondata[i].value != undefined){
      jsondata[i].id = jsondata[i].id+"";
      jsondata[i].value = jsondata[i].value+"";
      if(jsondata[i].info != undefined)
        jsondata[i].info = jsondata[i].info+"";
      new_arr.push(jsondata[i]);
    }
  }
  return new_arr;
}
// php analogs
function escapearg(s){
  if(s == undefined || !s) return '';
  return s.replace('\\','\\\\').
           replace('*','\\*').
           replace('.','\\.').
           replace('/','\\/');
}
function htmlspecialchars(s){
  if(s == undefined || !s) return '';
  return s.replace('&','&amp;').
           replace('<','&lt;').
           replace('>','&gt;');
}
function ltrim(s){
  if(s == undefined || !s) return '';
  return s.replace(/^\s+/g,'');
}

// extending jQuery
$.fn.autocomplete = function(options){ return this.each(function(){
  // take me
  var me = $(this);
  var me_this = $(this).get(0);

  // test for supported text elements
  if(!me.is('input:text,input:password,textarea'))
  return;

  // get or ajax_get required!
  if(!options && (!$.isFunction(options.get) || !options.ajax_get)){
  return;
  }  
  // check plugin enabled
  if(me.attr('jqac') == 'on') return;

  // plugin on!
  me.attr('jqac','on');

  // no browser's autocomplete!
  me.attr('autocomplete','off');

  // default options
  options = $.extend({ 
                      delay     : 500 ,
                      timeout   : 5000 ,
                      minchars  : 3 ,
                      multi     : false ,
                      cache     : true , 
                      height    : 150 ,
                      autowidth : false ,
                      noresults : 'No results'
                      },
                      options);

  // bind key events
  // handle special keys here
  me.keydown(function(ev){
    switch(ev.which){
      // return choose highlighted item or default propogate
      case RETURN:
        if(!suggestions_menu) return true;
        else setHighlightedValue();
        return false;
      // escape clears menu
      case ESC:
        clearSuggestions();
        return false;
    }
    return true;
  });
  me.keypress(function(ev){
    // ev.which doesn't work here - it always returns 0
    switch(ev.keyCode){
      case RETURN: case ESC:
        return false;
      // up changes highlight
      case ARRUP:
        changeHighlight(ev.keyCode);
        return false;
      // down changes highlight or open new menu
      case ARRDOWN:
        if(!suggestions_menu) getSuggestions(getUserInput());
        else changeHighlight(ev.keyCode);
        return false;
     }
     return true;
  });
  // handle normal characters here
  me.keyup(function(ev) {
      switch(ev.which) {
        case RETURN: case ESC: case ARRLEFT: case ARRRIGHT: case ARRUP: case ARRDOWN:
          return false;
        default:
          getSuggestions(getUserInput());
      }
      return true;
  });

  // init variables
  var user_input = "";
  var input_chars_size  = 0;
  var suggestions = [];
  var current_highlight = 0;
  var suggestions_menu = false;
  var suggestions_list = false;
  var loading_indicator = false;
  var clearSuggestionsTimer = false;
  var getSuggestionsTimer = false;
  var showLoadingTimer = false;
  var zIndex = me.css('z-index');

  // get user input
  function getUserInput(){
    var val = me.val();
    if(options.multi){
      var pos = getCaretPosition(me_this);
      var start = pos.start;
      for(;start>0 && val.charAt(start-1) != ',';start--){}
      var end = pos.start;
      for(;end<val.length && val.charAt(end) != ',';end++){}
      var val = val.substr(start,end-start);
    }
    return ltrim(val);
  }
  // set suggestion
  function setSuggestion(val){
    user_input = val;
    if(options.multi){
      var orig = me.val();
      var pos = getCaretPosition(me_this);
      var start = pos.start;
      for(;start>0 && orig.charAt(start-1) != ',';start--){}
      var end = pos.start;
      for(;end<orig.length && orig.charAt(end) != ',';end++){}
      var new_val = orig.substr(0,start) + (start>0?' ':'') + val + orig.substr(end);
      me.val(new_val);
      setCaret(me_this,start + val.length + (start>0?1:0));
    }
    else{
      me_this.focus();
      me.val(val);
    }
  }
  // get suggestions
  function getSuggestions(val){
    // input length is less than the min required to trigger a request
    // reset input string
    // do nothing
    if (val.length < options.minchars){
      clearSuggestions();
      return false;
    }
    // if caching enabled, and user is typing (ie. length of input is increasing)
    // filter results out of suggestions from last request
    if (options.cache && val.length > input_chars_size && suggestions.length){
      var arr = [];
      for (var i=0;i<suggestions.length;i++){
        var re = new RegExp("("+escapearg(val)+")",'ig');
        if(re.exec(suggestions[i].value))
          arr.push( suggestions[i] );
      }
      user_input = val;
      input_chars_size = val.length;
      suggestions = arr;
      createList(suggestions);
      return false;
    }
    else{// do new request
      clearTimeout(getSuggestionsTimer);
      user_input = val;
      input_chars_size = val.length;
      getSuggestionsTimer = setTimeout( 
        function(){ 
          suggestions = [];
          // call pre callback, if exists
          if($.isFunction(options.pre_callback))
            options.pre_callback();
          // call get
          if($.isFunction(options.get)){
            suggestions = prepareArray(options.get(val));
            createList(suggestions);
          }
          // call AJAX get
          else if($.isFunction(options.ajax_get)){
            clearSuggestions();
            showLoadingTimer = setTimeout(show_loading,options.delay);
            options.ajax_get(val,ajax_continuation);
          }
        },
        options.delay );
    }
    return false;
  };
  // AJAX continuation
  function ajax_continuation(jsondata){
    hide_loading();
    suggestions = prepareArray(jsondata);
    createList(suggestions);
  }
  // shows loading indicator
  function show_loading(){
    if(!loading_indicator){
      loading_indicator = $('<div class="jqac-menu"><div class="jqac-loading">Loading</div></div>').get(0);
      $(loading_indicator).css('position','absolute');
      var pos = me.offset();
      $(loading_indicator).css('left', pos.left + "px");
      $(loading_indicator).css('top', ( pos.top + me.height() + 2 ) + "px");
      if(!options.autowidth)
        $(loading_indicator).width(me.width());
      $('body').append(loading_indicator);
    }
    $(loading_indicator).show();
    setTimeout(hide_loading,10000);
  }
  // hides loading indicator 
  function hide_loading(){
    if(loading_indicator)
      $(loading_indicator).hide();
    clearTimeout(showLoadingTimer);
  }
  // create suggestions list
  function createList(arr){
    if(suggestions_menu)
      $(suggestions_menu).remove();
    hide_loading();
    killTimeout();

    // create holding div
    suggestions_menu = $('<div class="jqac-menu"></div>').get(0);

    // ovveride some necessary CSS properties 
    $(suggestions_menu).css({'position':'absolute',
                             'z-index':zIndex,
                             'max-height':options.height+'px',
                             'overflow-y':'auto'});

    // create and populate ul
    suggestions_list = $('<ul></ul>').get(0);
    // set some CSS's
    $(suggestions_list).
      css('list-style','none').
      css('margin','0px').
      css('padding','2px').
      css('overflow','hidden');
    // regexp for replace 
    var re = new RegExp("("+escapearg(htmlspecialchars(user_input))+")",'ig');
    // loop throught arr of suggestions creating an LI element for each suggestion
    for (var i=0;i<arr.length;i++){
      var val = new String(arr[i].value);
      // using RE
      var output = htmlspecialchars(val).replace(re,'<em>$1</em>');
      // using substr
      //var st = val.toLowerCase().indexOf( user_input.toLowerCase() );
      //var len = user_input.length;
      //var output = val.substring(0,st)+"<em>"+val.substring(st,st+len)+"</em>"+val.substring(st+len);

      var span = $('<span class="jqac-link">'+output+'</span>').get(0);
      if (arr[i].info != undefined && arr[i].info != ""){
        $(span).append($('<div class="jqac-info">'+arr[i].info+'</div>'));
      }

      $(span).attr('name',i+1);
      $(span).click(function () { setHighlightedValue(); });
      $(span).mouseover(function () { setHighlight($(this).attr('name'),true); });

      var li = $('<li></li>').get(0);
      $(li).append(span);

      $(suggestions_list).append(li);
    }

    // no results
    if (arr.length == 0){
      $(suggestions_list).append('<li class="jqac-warning">'+options.noresults+'</li>');
    }

    $(suggestions_menu).append(suggestions_list);

    // get position of target textfield
    // position holding div below it
    // set width of holding div to width of field
    var pos = me.offset();

    $(suggestions_menu).css('left', pos.left + "px");
    $(suggestions_menu).css('top', ( pos.top + me.height() + 2 ) + "px");
    if(!options.autowidth)
      $(suggestions_menu).width(me.width());

    // set mouseover functions for div
    // when mouse pointer leaves div, set a timeout to remove the list after an interval
    // when mouse enters div, kill the timeout so the list won't be removed
    $(suggestions_menu).mouseover(function(){ killTimeout() });
    $(suggestions_menu).mouseout(function(){ resetTimeout() });

    // add DIV to document
    $('body').append(suggestions_menu);

    // bgIFRAME support
    if($.fn.bgiframe)
      $(suggestions_menu).bgiframe({height: suggestions_menu.scrollHeight});


    // adjust height: add +20 for scrollbar
    if(suggestions_menu.scrollHeight > options.height){
      $(suggestions_menu).height(options.height);
      $(suggestions_menu).width($(suggestions_menu).width()+20);
    }
	
    // currently no item is highlighted
    current_highlight = 0;

    // remove list after an interval
    clearSuggestionsTimer = setTimeout(function () { clearSuggestions() }, options.timeout);
  };
  // set highlighted value
  function setHighlightedValue(){
    if(current_highlight && suggestions[current_highlight-1]){
      var sugg = suggestions[ current_highlight-1 ];
      if(sugg.affected_value != undefined && sugg.affected_value != '')
        setSuggestion(sugg.affected_value);
      else
        setSuggestion(sugg.value);
      // pass selected object to callback function, if exists
      if ($.isFunction(options.callback))
        options.callback( suggestions[current_highlight-1] );

      clearSuggestions();
    }
  };
  // change highlight according to key
  function changeHighlight(key){	
    if(!suggestions_list || suggestions.length == 0) return false;
    var n;
    if (key == ARRDOWN)
      n = current_highlight + 1;
    else if (key == ARRUP)
      n = current_highlight - 1;

    if (n > $(suggestions_list).children().size())
      n = 1;
    if (n < 1)
      n = $(suggestions_list).children().size();
    setHighlight(n);
  };
  // change highlight
  function setHighlight(n,mouse_mode){
    if (!suggestions_list) return false;
    if (current_highlight > 0) clearHighlight();
    current_highlight = Number(n);
    var li = $(suggestions_list).children().get(current_highlight-1);
    li.className = 'jqac-highlight';
    // for mouse mode don't adjust scroll! prevent scrolling jumps
    if(!mouse_mode) adjustScroll(li);
    killTimeout();
  };
  // clear highlight
  function clearHighlight(){
    if (!suggestions_list)return false;
    if (current_highlight > 0){
      $(suggestions_list).children().get(current_highlight-1).className = '';
      current_highlight = 0;
    }
  };
  // clear suggestions list
  function clearSuggestions(){
    killTimeout();
    if(suggestions_menu){
      $(suggestions_menu).remove();
      suggestions_menu = false;
      suggestions_list = false;
      current_highlight = 0;
    }
  };
  // set scroll
  function adjustScroll(el){
    if(!suggestions_menu) return false;
    var viewportHeight = suggestions_menu.clientHeight;        
    var wholeHeight = suggestions_menu.scrollHeight;
    var scrolled = suggestions_menu.scrollTop;
    var elTop = el.offsetTop;
    var elBottom = elTop + el.offsetHeight;
    if(elBottom > scrolled + viewportHeight){
      suggestions_menu.scrollTop = elBottom - viewportHeight;
    }
    else if(elTop < scrolled){
      suggestions_menu.scrollTop = elTop;
    }
    return true; 
  }
  // timeout funcs
  function killTimeout(){
    clearTimeout(clearSuggestionsTimer);
  };
  function resetTimeout(){
    clearTimeout(clearSuggestionsTimer);
    clearSuggestionsTimer = setTimeout(function () { clearSuggestions() }, 1000);
  };

})};

})($);
