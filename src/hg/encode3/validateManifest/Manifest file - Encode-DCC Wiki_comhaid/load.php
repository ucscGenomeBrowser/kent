jQuery(function($){$('div.vectorMenu').each(function(){var self=this;$('h5:first a:first',this).click(function(e){$('.menu:first',self).toggleClass('menuForceShow');e.preventDefault();}).focus(function(){$(self).addClass('vectorMenuFocus');}).blur(function(){$(self).removeClass('vectorMenuFocus');});});});;mw.loader.state({"skins.vector":"ready"});

/* cache key: wiki:resourceloader:filter:minify-js:7:0fcba82177db6429d02096ea1f0465ed */
