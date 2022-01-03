These are the docker files for the "docker build" comment.

You only need to download the Dockerfile to a new directory and build the docker image:

    mkdir browserDocker && cd browserDocker
    wget https://raw.githubusercontent.com/ucscGenomeBrowser/kent/master/src/product/installer/docker/Dockerfile
    docker build .

The Apache server is running on port 80 then.
