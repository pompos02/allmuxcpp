FROM debian:12-slim

RUN printf 'deb http://deb.debian.org/debian testing main\n' > /etc/apt/sources.list.d/testing.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        git \
        ninja-build \
        xz-utils \
    && apt-get install -y --no-install-recommends -t testing \
        gcc-14 \
        g++-14 \
        libstdc++-14-dev \
        libboost-dev \
    && curl -fsSL https://github.com/Kitware/CMake/releases/download/v3.30.9/cmake-3.30.9-linux-x86_64.tar.gz \
        | tar -xz -C /opt \
    && ln -s /opt/cmake-3.30.9-linux-x86_64/bin/cmake /usr/local/bin/cmake \
    && rm -rf /var/lib/apt/lists/*

ENV CC=gcc-14
ENV CXX=g++-14
