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
    pkg-config \
    wget \
    ca-certificates \
    libpng-dev \
    libssl-dev \
    libbz2-dev \
    liblzma-dev \
    zlib1g-dev \
    libmysqlclient-dev \
    uuid-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

##
# libcurl, built here from source as a static library.  refs #38153
#
# Since the htslib submodule arrived (refs #37741) every userApps binary links
# -lcurl, and the userApps link is SEMI_STATIC, so it needs libcurl.a.  Ubuntu
# has no such thing: libcurl4-openssl-dev ships only the shared library, and
# rebuilding the distro libcurl does not help either, because it wants krb5 and
# openldap, and libkrb5-dev has no static libraries at all.
#
# So build a deliberately small libcurl: http and https over OpenSSL, ftp, and
# gzip transfer encoding.  Its only dependencies are libssl, libcrypto and
# libz, which the userApps link already has as static libraries.  Everything
# else is switched off, both to drop the unsatisfiable krb5/ldap dependency and
# to keep the surface of a library we ship inside binaries as small as we can.
#
# The installed libcurl.a is replaced by a linker script that pulls in libssl
# and libcrypto along with it.  A shared libcurl records its own dependencies
# and a static one cannot, and not every link line here puts -lssl -lcrypto
# after -lcurl -- htslib's own tabix, bgzip and test programs do not -- so
# without this the switch to a static libcurl breaks them.  GROUP tells the
# linker to re-scan the three archives until nothing is left undefined, which
# also settles the circular reference between libssl and libcrypto.
##
ARG CURL_VERSION=8.21.0
ARG CURL_SHA256=d9b327997999045a24cda50f3983e69e51c516bd8be6ef9842fc7f99135e33bb
RUN cd /tmp && \
    wget -q https://curl.se/download/curl-${CURL_VERSION}.tar.gz && \
    echo "${CURL_SHA256}  curl-${CURL_VERSION}.tar.gz" | sha256sum -c - && \
    tar xzf curl-${CURL_VERSION}.tar.gz && \
    cd curl-${CURL_VERSION} && \
    ./configure --prefix=/usr/local \
        --disable-shared --enable-static \
        --with-openssl --with-zlib \
        --without-gssapi --disable-ldap --disable-ldaps \
        --without-libpsl --without-libidn2 --without-nghttp2 \
        --without-brotli --without-zstd \
        --without-librtmp --without-libssh2 --without-libssh \
        --disable-dict --disable-gopher --disable-imap --disable-mqtt \
        --disable-pop3 --disable-rtsp --disable-smb --disable-smtp \
        --disable-telnet --disable-tftp \
        > configure.log 2>&1 && \
    make -j $(nproc) > make.log 2>&1 && \
    make install > install.log 2>&1 && \
    test -e /usr/local/lib/libcurl.a && \
    mv /usr/local/lib/libcurl.a /usr/local/lib/libcurl_openssl.a && \
    printf 'GROUP ( %s %s %s )\n' /usr/local/lib/libcurl_openssl.a \
        "$(gcc -print-file-name=libssl.a)" "$(gcc -print-file-name=libcrypto.a)" \
        > /usr/local/lib/libcurl.a && \
    cd /tmp && rm -rf curl-${CURL_VERSION} curl-${CURL_VERSION}.tar.gz

# Set umask for both bash and commands from docker run
RUN echo 'umask 0002' >> /etc/profile && \
    echo '#!/bin/bash' > /entrypoint.sh && \
    echo 'umask 0002' >> /entrypoint.sh && \
    echo 'exec "$@"' >> /entrypoint.sh && \
    chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]

WORKDIR /home

