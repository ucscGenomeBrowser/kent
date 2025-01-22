##
# docker specification for image to build user apps
#
# By default, this will run as qateam, however it can also run
# as the current users with:
#
#  docker build --build-arg USERNAME=$(whoami) \
#               --build-arg GROUPNAME=$(whoami) \
#               --build-arg UID=$(id -u) \
#               --build-arg GID=$(id -g) \
#               $(realpath kent):/home/kent \
#               -t user-apps-build .
#
##

FROM ubuntu:22.04

ARG USERNAME=qateam
ARG GROUPNAME=genecats
ARG UID=30009
ARG GID=1305

ENV DEBIAN_FRONTEND=noninteractive

RUN groupadd --gid $GID $GROUPNAME && \
    useradd --uid $UID --gid $GID --create-home $USERNAME

RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    make \
    git \
    rsync \
    file \
    libtree \
    libc-bin \
    libc6-dev \
    net-tools \
    libpng-dev \
    libssl-dev \
    libbz2-dev \
    libmysqlclient-dev \
    uuid-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Set umask for both bash and commands from docker run
RUN echo 'umask 0002' >> /etc/profile && \
    echo '#!/bin/bash' > /entrypoint.sh && \
    echo 'umask 0002' >> /entrypoint.sh && \
    echo 'exec "$@"' >> /entrypoint.sh && \
    chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]

WORKDIR /home/

