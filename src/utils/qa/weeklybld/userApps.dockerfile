##
# docker specification for image to build user apps
##

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

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
    libcurl4-openssl-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Set umask for both bash and commands from docker run
RUN echo 'umask 0002' >> /etc/profile && \
    echo '#!/bin/bash' > /entrypoint.sh && \
    echo 'umask 0002' >> /entrypoint.sh && \
    echo 'exec "$@"' >> /entrypoint.sh && \
    chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]

WORKDIR /home

