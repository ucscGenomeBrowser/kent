$(document).ready(function() 
{
    var opt = {
        cacheLength: false,
        delay: 200,
        max:30
    };
    $("#org").autocomplete("../cgi-bin/chooseorg",opt);

});
