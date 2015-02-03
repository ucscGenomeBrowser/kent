/** @jsx React.DOM */
/* global PathUpdate */
var pt = React.PropTypes;

var Sortable = React.createClass({
    // This wraps a JQuery Sortable around child elements; when a child is dragged
    // and dropped, this simply cancels the JQuery action, calls update,
    // and assumes that the parent will change state and trigger a re-render.
    // Don't treat this as Immutable because it wraps around children that have their own props.

    mixins: [PathUpdate],
    // update(path, reordering) called when user moves a child.
    // reordering is an array whose indices are new positions and values are old pos.

    propTypes: { // Optional:
                 sortableConfig: pt.object,  // extra configuration settings for JQuery Sortable
                 idPrefix: pt.string,        // prefix for forming unique child-wrapper keys
                 className: pt.string,       // extra class(es) for wrapper div
                 style: pt.object            // extra style directives for wrapper div
               },

    getDefaultProps: function () {
        return { sortableConfig: {},
                 idPrefix: 's0rtableThingie'
               };
    },

    // Adapted from Angie's jsfiddle http://jsfiddle.net/kog0ur0u/6/
    // which in turn uses ideas and code from
    // https://groups.google.com/d/msg/reactjs/mHfBGI3Qwz4/_vnwOellnyoJ

    intToId: function(i) {
        // Make a string that encodes an int for later use.
        return this.props.idPrefix + i;
    },

    idToInt: function(id) {
        // Extract an int from a string made by intToId.
        return parseInt(id.substring(this.props.idPrefix.length));
    },

    handleDrop: function () {
        // JQuery Sortable 'toArray' gives an array of element ids in the new sequence.
        // Convert that to an array whose indices are new positions and values are old
        // positions, tell JQuery to cancel, and call update with the array.
        var reordering = this.$jq.sortable('toArray').map(this.idToInt);
        this.$jq.sortable('cancel');
        var path = this.props.path || [];
        this.props.update(path, reordering);
    },

    componentDidMount: function () {
        // Now that we have a DOM node, make it a JQueryUi Sortable:
        var config = _.clone(this.props.sortableConfig);
        config.stop = this.handleDrop;
        this.$jq = $(this.getDOMNode());
        this.$jq.sortable(config);
    },

    render: function () {
        // Wrap each child in a div with an id that encodes the original order.
        var wrappedChildren = this.props.children.map(function(child, ix) {
            var id = this.intToId(ix);
            return <div id={id} key={id}>{child}</div>;
        }.bind(this));
        return (
            <div className={this.props.className} style={this.props.style}>
              {wrappedChildren}
            </div>
        );
    }

});

// Without this, jshint complains that Sortable is not used.  Module system would help.
Sortable = Sortable;
