FROM debian:jessie

# Add Tini
COPY tini /tini
ENTRYPOINT ["/tini", "-g", "--"]
CMD ["/bin/sh"]

ENV LANG=C.UTF-8

# we need non-free for xml2rfc
RUN sed -i "s/jessie main/jessie main non-free/" /etc/apt/sources.list \
 && apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    automake \
    build-essential \
    desktop-file-utils \
    git \
    libglib2.0-dev \
    libtool \
    python \
    python-dateutil \
    python-dev \
    python-lxml \
    python-m2crypto \
    python-pil \
    python-setuptools \
    python-yaml \
    socat \
    xml2rfc \
 && rm -rf /var/lib/apt/lists/*

ADD opendiamond-HEAD.tar.gz /usr/src/opendiamond

RUN cd /usr/src/opendiamond && autoreconf -f -i \
 && ./configure && make install && make clean \
 && ldconfig

COPY extract-filters /usr/local/bin
