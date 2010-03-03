$(document).ready(function() 
{
    var opt = {
        cacheLength: false,
        delay: 200,
    };
    $("#org").autocomplete("../cgi-bin/autocomplete",opt);

});
