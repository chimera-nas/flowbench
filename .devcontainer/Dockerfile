FROM ubuntu:24.04

ARG ENABLE_XLIO=1

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get -y update && \
    apt-get -y --no-install-recommends upgrade && \
    apt-get -y --no-install-recommends install unminimize && \
    echo y | unminimize && \
    apt-get -y --no-install-recommends install clang clang-tools cmake ninja-build git lldb gdb less psmisc uncrustify \
    autoconf automake make libtool pkg-config vim docker.io sudo ncurses-bin \
    net-tools tshark tcpdump uuid-dev iproute2 man-db manpages-dev ca-certificates ssh libjansson-dev libclang-rt-18-dev llvm \
    libxxhash-dev liburcu-dev librdmacm-dev liburing-dev libunwind-dev flex bison libncurses-dev libcurl4-openssl-dev build-essential ruby-full && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*


RUN if [ "$ENABLE_XLIO" = "1" ] ; then \
    git clone https://github.com/Mellanox/libdpcp.git /libdpcp && \
    cd /libdpcp && \
    ./autogen.sh && \
    ./configure && \
    make -j8 && \
    make install && \
    git clone https://github.com/Mellanox/libxlio.git /libxlio && \
    cd /libxlio && \
    git checkout 9877e909310cde1112744206cfeb04524910c758 && \
    ./autogen.sh && \
    ./configure --with-dpcp=/usr/local && \
    make -j8 && \
    make install ; \
    fi
