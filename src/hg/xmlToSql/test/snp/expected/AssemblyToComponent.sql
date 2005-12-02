CREATE TABLE AssemblyToComponent (
    Assembly int not null,
    Component int not null,
    INDEX(Assembly),
    INDEX(Component)
);
