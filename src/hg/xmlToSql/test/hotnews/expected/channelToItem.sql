CREATE TABLE channelToItem (
    channel int not null,
    item int not null,
    INDEX(channel),
    INDEX(item)
);
