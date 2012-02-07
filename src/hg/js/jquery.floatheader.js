/*
	jQuery floating header plugin v1.4.0
	Licenced under the MIT License	
	Copyright (c) 2009, 2010, 2011 
		Erik Bystrom <erik.bystrom@gmail.com>

	Contributors:
		Elias Bergqvist <elias@basilisk.se>
		Diego Arbelaez <diegoarbelaez@gmail.com>
		Glen Gilbert	
		Vasilianskiy Sergey		
		Stephen J. Fuhry
      Jason Axley
*/ 
(function($){
	/**
	 * Clone the table header floating and binds its to the browser scrolling
	 * so that it will be displayed when the original table header is out of sight.
	 *
	 * The plugin defines two function on the table element.
	 * 	fhRecalculate	Recalculates with column widths of the floater.
	 *	fhInit			Recreates the floater from the source table header.
	 *
	 * @param config
	 *		An optional dictionary with configuration for the plugin.
	 *		
	 *		fadeOut		The length of the fade out animation in ms. Default: 200
	 *		fadeIn		The length of the face in animation in ms. Default: 200
	 *		forceClass	Forces the plugin to use the markerClass instead of thead. Default: false
	 *		markerClass The classname to use when marking which table rows that should be floating. Default: floating
	 *		floatClass	The class of the div that contains the floating header. The style should
	 *					contain an appropriate z-index value. Default: 'floatHeader'
	 *		cbFadeOut	A callback that is called when the floating header should be faded out.
	 *					The method is called with the wrapped header as argument.
	 *		cbFadeIn	A callback that is called when the floating header should be faded in.
	 *					The method is called with the wrapped header as argument.
	 *		recalculate	Recalculate the column width on every scroll event
	 *
	 * @version 1.4.0
    * @see http://blog.slackers.se/2009/07/jquery-floating-table-header-plugin.html
	 */
	$.fn.floatHeader = function(config) {
		config = $.extend({
			fadeOut: 200,
			fadeIn: 200,
			forceClass: false,
			markerClass: 'floating',
			floatClass: 'floatHeader',
			recalculate: false,
			IE6Fix_DetectScrollOnBody: true
		}, config);	
		
		return this.each(function () {	
			var self = $(this);
           
         var tableClone = self[0].cloneNode(false);  // only perform a shallow copy
         var table = $(tableClone);
         var cloneId = table.attr("id") + "FloatHeaderClone";
         table.attr("id", cloneId); // change the ID to avoid conflicts
         table.parent().remove();   // remove any existing float box divs for this same grid.  we may be reinitializing and don't want to keep adding these to the DOM

			self.floatBox = $('<div class="'+config.floatClass+'"style="display:none"></div>');
			self.floatBox.append(table);
			
			// Fix for the IE resize handling
			self.IEWindowWidth = document.documentElement.clientWidth;
			self.IEWindowHeight = document.documentElement.clientHeight;
			
         // DO NOT create the floater yet.  
         // Lazy-load and create it only when neccessary to improve page load time

			/*
			 * This is very specific to IE6 only if using position:fixed fixes.
			 * This requires the window overflow to be set to hidden and the
			 * containing 'body' tag to have overflow:auto.
			 */
			if (!$.browser.msie) {
				config.IE6Fix_DetectScrollOnBody = false;
			} else {
				if ($.browser.version > 7) {
					config.IE6Fix_DetectScrollOnBody = false;
				}
			}
			var scrollElement = config.IE6Fix_DetectScrollOnBody ? $('body') : $(window);
			
			// bind to the scroll event
			scrollElement.scroll(function() {		
				if (self.floatBoxVisible) {		
					if (!showHeader(self, self.floatBox)) {
						// kill the floatbox			
						var offset = self.offset();
						self.floatBox.css('position', 'absolute');
						self.floatBox.css('top', offset.top);
						self.floatBox.css('left', offset.left);					
						
						self.floatBoxVisible = false;
						if (config.cbFadeOut) {
							config.cbFadeOut(self.floatBox);
						} else {
							self.floatBox.stop(true, true);
							self.floatBox.fadeOut(config.fadeOut);
						}					
					}
            } else if (showHeader(self, self.floatBox)) {
               // populate the floating header now in case it is needed (lazy load)
               // and only if we haven't yet filled in the header details 
               if (table.children().length === 0) {
                  createFloater(table, self, config);
               }                  
						
					self.floatBoxVisible = true;
					// show the floatbox
					if ($.browser.msie && $.browser.version < 7) {
						// IE6 can't handle fixed positioning; has to use absolute and additional calculation to position correctly
						// strictly speaking, this isn't necessary as it is position:absolute
						self.floatBox.css('position', 'absolute'); 
					} else {
						self.floatBox.css('position', 'fixed');
					}
					
					if (config.cbFadeIn) {
						config.cbFadeIn(self.floatBox);
					} else {
						self.floatBox.stop(true, true);					
						self.floatBox.fadeIn(config.fadeIn);
					}
				}
				
				// if the box is visible update the position
				if (self.floatBoxVisible) {
					// ie6 fix
					if ($.browser.msie && $.browser.version <= 7) {
						self.floatBox.css('top', $(window).scrollTop());
					} else {
						self.floatBox.css('top', 0);
					}
					self.floatBox.css('left', self.offset().left-$(window).scrollLeft());			
					if (config.recalculate) {		
						recalculateColumnWidth(table, self, config);
					}
				}
			});
			
			/*
			 * Unfortunately IE gets rather stroppy with the non-IE version,
			 * constantly resizing, thus cooking your CPU with 100% usage whilest
			 * the browser crashes. So, test for IE and add additional code.
			 */
			if ($.browser.msie && $.browser.version <= 7) {
				$(window).resize(function() {
					// Check if the window size has changed ()
					if ((self.IEWindowWidth != document.documentElement.clientWidth) || (self.IEWindowHeight != document.documentElement.clientHeight)) {
						// Update the client width and height with the Microsoft version.
						self.IEWindowWidth = document.documentElement.clientWidth;
						self.IEWindowHeight = document.documentElement.clientHeight;

                  if (table.children().length > 0) {
                     table.fastempty();
                     createFloater(table, self, config);
                  }
					}
				});
			} else {
				// bind to the resize event
				$(window).resize(function() {
 	            // Only redo the header cells if we have created them already
               if (table.children().length > 0) {
                  table.fastempty();
                  createFloater(table, self, config);
               }
				});
			};			

			// append the floatBox to the dom
        	$(self).after(self.floatBox);			
        	
        	// connect some convenience callbacks
			this.fhRecalculate = function() {
				recalculateColumnWidth(table, self, config);
			};
			
			this.fhInit = function() {
         	// Only redo the header cells if we have created them already
            if (table.children().length > 0) {
                table.fastempty();
                createFloater(table, self, config);
            }	
			};

         /// Creating an alternative to the jquery empty() API that is optimized for cases where you know that there are not any event handlers left on the nodes in the container you are emptying
         /// Otherwise, you could experience memory leaks.  empty() is very slow because it has to visit every DOM element and delete it individually.
         /// This function will clear out all child elements using DOM APIs. Note:  you CANNOT use innerHTML = '' as a general solution because in IE innerHTML is read-only for many, many container nodes.
         $.fn.fastempty = function() {
           if (this[0]) {
              while (this[0].hasChildNodes()) {
                   this[0].removeChild(this[0].lastChild);
               }
           }

           return this;
         };
		});
	};
	
	/**
	 * Copies the template and inserts each element into the target.
	 */
	function createFloater(target, template, config) {
		target.width(template.width());
		
		var items;
		if (!config.forceClass && template.children('thead').length > 0) {
			// set the template to the children of thead
			items = template.children('thead').eq(0).children();
			var thead = jQuery("<thead/>");
			target.append(thead);
			target = thead;
		} else {
			// set the template to the class marking
			items = template.find('.'+config.markerClass);
		}		
		
		// iterate though each row that should be floating
		items.each(function() {
			var row = $(this);
         // avoid deep clone, then removal the nodes you just cloned
         var rowClone = row[0].cloneNode(false); 
         var floatRow = $(rowClone);

			// adjust the column width for each header cell
			row.children().each(function() {
				var cell = $(this);
				var floatCell = cell.clone();
				
				floatCell.width(cell.width());
				floatRow.append(floatCell);
			});

			// append the row to the table
			target.append(floatRow);
		});	
	}
	
	/**
	 * Recalculates the column widths of the floater.
	 */
	function recalculateColumnWidth(target, template, config) {
		target.width(template.width());
		var src;
		var dst;
		if (!config.forceClass && template.children('thead').length > 0) {
			src = template.children('thead').eq(0).children().eq(0);
			dst = target.children('thead').eq(0).children().eq(0);
		} else {
			src = template.find('.'+config.markerClass).eq(0);
			dst = target.children().eq(0);
		}
		
		dst = dst.children().eq(0);
		src.children().each(function(index, element) {
			dst.width($(element).width());			
			dst = dst.next();
		});
	}
	
	/**
	 * Determines if the element is visible
	 */
	function showHeader(element, floater) {
		var elem = $(element);
		var top = $(window).scrollTop();
		var y0 = elem.offset().top;
		
		var height = elem.height()-floater.height();
		var foot = elem.children('tfoot');
		if (foot.length > 0) {
			height -= foot.height();
		}
		
		return y0 <= top && top <= y0 + height;
	}
})(jQuery);
