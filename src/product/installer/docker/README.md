These are the docker files for the "docker build" command.

You only need to download the Dockerfile to a new directory and build the docker image. This works on Windows, OSX and Linux, as long as Docker is installed:

    mkdir browserDocker && cd browserDocker
    wget https://raw.githubusercontent.com/ucscGenomeBrowser/kent/master/src/product/installer/docker/Dockerfile
    docker build . -t gbImage

You can then run the gbImage image that you just built as a new container in daemon mode (-d) and export its port to localhost:

    docker run -d --name genomeBrowser -p 8080:80 gbImage

The Apache server is running on port 8080 then and you should be able to access it via https://localhost:8080
