FROM fedora:34

RUN dnf update -y \
    && dnf install -y autoconf-archive \
                      brotli-devel \
                      clang-analyzer \
                      git \
                      glib2-doc \
                      gnutls-devel \
                      gtk-doc \
                      httpd \
                      lcov \
                      libasan \
                      libpsl-devel \
                      libnghttp2-devel \
                      lsof \
                      make \
                      meson \
                      mod_ssl \
                      python2.7.x86_64 \
                      redhat-rpm-config \
                      samba-winbind-clients \
                      sysprof-devel \
                      valgrind \
                      which \
    && dnf clean all \
    && python2.7 -m ensurepip \
    && pip2.7 install virtualenv autobahntestsuite \
    && pip3 install quart

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG C.UTF-8
