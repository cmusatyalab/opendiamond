FROM centos:centos6

# Add Tini
COPY tini /tini
ENTRYPOINT ["/tini", "-g", "--"]
CMD ["/bin/sh"]

# Install epel (for xml2rfc)
RUN yum -y install epel-release

RUN yum -y install \
    automake \
    file \
    git \
    glib2-devel \
    glib2-devel.i686 \
    glibc-devel \
    glibc-devel.i686 \
    libgcc \
    libgcc.i686 \
    libstdc++-devel \
    libstdc++-devel.i686 \
    libtool \
    make \
    python-devel \
    python-requests \
    python-setuptools \
    python-xml2rfc \
    python2-simplejson \
    python-yaml \
    socat \
 && yum -y clean all

ADD opendiamond-HEAD.tar.gz /usr/src/opendiamond

RUN cd /usr/src/opendiamond && autoreconf -f -i \
 && ./configure CFLAGS=-m32 --prefix=/usr && make -C libfilter install && make clean \
 && ./configure --prefix=/usr --libdir=/usr/lib64 && make install && make clean \
 && ldconfig

COPY extract-filters /usr/local/bin
