/* JQuery Plugins + some code required for drop down menus
 * Must be loaded after jquery.js
 *
 * CONTENTS:
 * hoverIntent r5
 * Superfish v1.4.8 - jQuery menu widget
 * Superfish initialization (Add Superfish to all Nice menus with some basic options.)
 
 */

/**
* hoverIntent r5 // 2007.03.27 // jQuery 1.1.2+
* <http://cherne.net/brian/resources/jquery.hoverIntent.html>
* 
* hoverIntent is currently available for use in all personal or commercial 
* projects under both MIT and GPL licenses. This means that you can choose 
* the license that best suits your project, and use it accordingly.

* @param  f  onMouseOver function || An object with configuration options
* @param  g  onMouseOut function  || Nothing (use configuration options object)
* @author    Brian Cherne <brian@cherne.net>
*/
(function($){$.fn.hoverIntent=function(f,g){var cfg={sensitivity:7,interval:100,timeout:0};cfg=$.extend(cfg,g?{over:f,out:g}:f);var cX,cY,pX,pY;var track=function(ev){cX=ev.pageX;cY=ev.pageY;};var compare=function(ev,ob){ob.hoverIntent_t=clearTimeout(ob.hoverIntent_t);if((Math.abs(pX-cX)+Math.abs(pY-cY))<cfg.sensitivity){$(ob).off("mousemove",track);ob.hoverIntent_s=1;return cfg.over.apply(ob,[ev]);}else{pX=cX;pY=cY;ob.hoverIntent_t=setTimeout(function(){compare(ev,ob);},cfg.interval);}};var delay=function(ev,ob){ob.hoverIntent_t=clearTimeout(ob.hoverIntent_t);ob.hoverIntent_s=0;return cfg.out.apply(ob,[ev]);};var handleHover=function(e){var p=(e.type=="mouseover"?e.fromElement:e.toElement)||e.relatedTarget;while(p&&p!=this){try{p=p.parentNode;}catch(e){p=this;}}if(p==this){return false;}var ev=jQuery.extend({},e);var ob=this;if(ob.hoverIntent_t){ob.hoverIntent_t=clearTimeout(ob.hoverIntent_t);}if(e.type=="mouseover"){pX=ev.pageX;pY=ev.pageY;$(ob).on("mousemove",track);if(ob.hoverIntent_s!=1){ob.hoverIntent_t=setTimeout(function(){compare(ev,ob);},cfg.interval);}}else{$(ob).off("mousemove",track);if(ob.hoverIntent_s==1){ob.hoverIntent_t=setTimeout(function(){delay(ev,ob);},cfg.timeout);}}};return this.on("mouseover", handleHover).on("mouseout", handleHover);};})(jQuery);


/*
 * Superfish v1.4.8 - jQuery menu widget
 * Copyright (c) 2008 Joel Birch
 *
 * Dual licensed under the MIT and GPL licenses:
 * 	http://www.opensource.org/licenses/mit-license.php
 * 	http://www.gnu.org/licenses/gpl.html
 *
 * CHANGELOG: http://users.tpg.com.au/j_birch/plugins/superfish/changelog.txt
 */

;(function($){
	$.fn.superfish = function(op){

		var sf = $.fn.superfish,
			c = sf.c,
			$arrow = $(['<span class="',c.arrowClass,'"> &#187;</span>'].join('')),
			over = function(){
				var $$ = $(this), menu = getMenu($$);
				clearTimeout(menu.sfTimer);
				$$.showSuperfishUl().siblings().hideSuperfishUl();
			},
			out = function(){
				var $$ = $(this), menu = getMenu($$), o = sf.op;
				clearTimeout(menu.sfTimer);
				menu.sfTimer=setTimeout(function(){
					o.retainPath=($.inArray($$[0],o.$path)>-1);
					$$.hideSuperfishUl();
					if (o.$path.length && $$.parents(['li.',o.hoverClass].join('')).length<1){over.call(o.$path);}
				},o.delay);	
			},
			getMenu = function($menu){
				var menu = $menu.parents(['ul.',c.menuClass,':first'].join(''))[0];
				sf.op = sf.o[menu.serial];
				return menu;
			},
			addArrow = function($a){ $a.addClass(c.anchorClass).append($arrow.clone()); };
			
		return this.each(function() {
			var s = this.serial = sf.o.length;
			var o = $.extend({},sf.defaults,op);
			o.$path = $('li.'+o.pathClass,this).slice(0,o.pathLevels).each(function(){
				$(this).addClass([o.hoverClass,c.bcClass].join(' '))
					.filter('li:has(ul)').removeClass(o.pathClass);
			});
			sf.o[s] = sf.op = o;
			
			$('li:has(ul)',this)[($.fn.hoverIntent && !o.disableHI) ? 'hoverIntent' : 'hover'](over,out).each(function() {
				if (o.autoArrows) addArrow( $('>a:first-child',this) );
			})
			.not('.'+c.bcClass)
				.hideSuperfishUl();
			
			var $a = $('a',this);
			$a.each(function(i){
				var $li = $a.eq(i).parents('li');
				$a.eq(i).on("focus", function(){over.call($li);}).on("blur", function(){out.call($li);});
			});
			o.onInit.call(this);
			
		}).each(function() {
			var menuClasses = [c.menuClass];
			if (sf.op.dropShadows  && !($.browser.msie && $.browser.version < 7)) menuClasses.push(c.shadowClass);
			$(this).addClass(menuClasses.join(' '));
		});
	};

	var sf = $.fn.superfish;
	sf.o = [];
	sf.op = {};
	sf.c = {
		bcClass     : 'sf-breadcrumb',
		menuClass   : 'sf-js-enabled',
		anchorClass : 'sf-with-ul',
		arrowClass  : 'sf-sub-indicator',
		shadowClass : 'sf-shadow'
	};
	sf.defaults = {
		hoverClass	: 'sfHover',
		pathClass	: 'overideThisToUse',
		pathLevels	: 1,
		delay		: 800,
		animation	: {opacity:'show'},
		speed		: 'normal',
		autoArrows	: true,
		dropShadows : true,
		disableHI	: false,		// true disables hoverIntent detection
		onInit		: function(){}, // callback functions
		onBeforeShow: function(){},
		onShow		: function(){},
		onHide		: function(){}
	};
	$.fn.extend({
		hideSuperfishUl : function(){
			var o = sf.op,
				not = (o.retainPath===true) ? o.$path : '';
			o.retainPath = false;
			var $ul = $(['li.',o.hoverClass].join(''),this).add(this).not(not).removeClass(o.hoverClass)
					.find('>ul').hide().css('visibility','hidden');
			o.onHide.call($ul);
			return this;
		},
		showSuperfishUl : function(){
			var o = sf.op,
				sh = sf.c.shadowClass+'-off',
				$ul = this.addClass(o.hoverClass)
					.find('>ul:hidden').css('visibility','visible');
			o.onBeforeShow.call($ul);
			$ul.animate(o.animation,o.speed,function(){  o.onShow.call($ul); });
			return this;
		}
	});

})(jQuery);



// This uses Superfish 1.4.8
// (http://users.tpg.com.au/j_birch/plugins/superfish)
// Add Superfish to all Nice menus with some basic options.
(function ($) {
  $(document).ready(function() {
    $('ul.nice-menu').superfish({
      // Apply a generic hover class.
      hoverClass: 'over',
      // Disable generation of arrow mark-up.
      autoArrows: false,
      // Disable drop shadows.
      dropShadows: false,
      // Mouse delay.
      delay: 800,
      // Animation speed.
      speed: 1
    });
    $('ul.nice-menu ul').css('display', 'none');
  });
})(jQuery);


// Keyboard accessibility for menu bar disclosure buttons (WCAG 2.1)
(function ($) {
  $(document).ready(function() {
    var $menu = $('ul.nice-menu');
    if (!$menu.length) return;
    var suppressFocus = false; // prevent focus handler from interfering with click/escape

    // Track mouse clicks so focus handler can distinguish Tab from click
    $menu.on('mousedown', 'li.menuparent > button', function() {
      suppressFocus = true;
    });

    // Toggle dropdown when a disclosure button is clicked (Enter/Space/click)
    $menu.on('click', 'li.menuparent > button', function(e) {
      e.preventDefault();
      suppressFocus = false;
      var $li = $(this).parent();
      var isOpen = $li.hasClass('over');
      // Close all open menus first
      $menu.find('li.menuparent').removeClass('over')
        .find('> ul').hide().css('visibility', 'hidden');
      $menu.find('li.menuparent > button, li.menuparent > a')
        .attr('aria-expanded', 'false');
      if (!isOpen) {
        $li.addClass('over').find('> ul').show().css('visibility', 'visible');
        $(this).attr('aria-expanded', 'true');
      }
    });

    // Focus on a button shows its dropdown (Tab navigation only, not mouse click)
    $menu.on('focus', 'li.menuparent > button', function() {
      if (suppressFocus) { suppressFocus = false; return; }
      var $li = $(this).parent();
      $li.showSuperfishUl().siblings().hideSuperfishUl();
      $(this).attr('aria-expanded', 'true');
      $li.siblings().find('> button, > a').attr('aria-expanded', 'false');
    });

    $menu.on('blur', 'li.menuparent > button', function() {
      var $li = $(this).parent();
      var o = $.fn.superfish.op;
      var delay = (o && o.delay) ? o.delay : 800;
      var menu = $menu[0];
      clearTimeout(menu.sfTimer);
      var $btn = $(this);
      menu.sfTimer = setTimeout(function() {
        $li.hideSuperfishUl();
        $btn.attr('aria-expanded', 'false');
      }, delay);
    });

    // Sync aria-expanded for <a>-based parent items (Genomes, Genome Browser)
    $menu.on('focus', 'li.menuparent > a[aria-expanded]', function() {
      $(this).attr('aria-expanded', 'true');
    });

    $menu.on('blur', 'li.menuparent > a[aria-expanded]', function() {
      var $a = $(this);
      setTimeout(function() {
        if (!$a.parent().hasClass('over')) {
          $a.attr('aria-expanded', 'false');
        }
      }, 900);
    });

    // Sync aria-expanded on mouse hover for all parent items
    $menu.on('mouseenter', 'li.menuparent', function() {
      $(this).find('> button, > a[aria-expanded]').attr('aria-expanded', 'true');
    });

    $menu.on('mouseleave', 'li.menuparent', function() {
      var $trigger = $(this).find('> button, > a[aria-expanded]');
      var o = $.fn.superfish.op;
      var delay = (o && o.delay) ? o.delay : 800;
      setTimeout(function() {
        if (!$trigger.parent().hasClass('over')) {
          $trigger.attr('aria-expanded', 'false');
        }
      }, delay + 100);
    });

    // Escape key closes any open dropdown and returns focus to its trigger
    $(document).on('keydown', function(e) {
      if (e.key === 'Escape' || e.keyCode === 27) {
        var $openLi = $menu.find('li.menuparent.over');
        if ($openLi.length) {
          $openLi.removeClass('over').find('> ul').hide().css('visibility', 'hidden');
          var $trigger = $openLi.find('> button, > a').first();
          $trigger.attr('aria-expanded', 'false');
          suppressFocus = true; // prevent focus handler from re-opening
          $trigger.focus();
          e.stopPropagation();
        }
      }
    });
  });
})(jQuery);
