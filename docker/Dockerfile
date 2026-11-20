FROM alpine:edge

# Install pmbootstrap, development utilities and kernel build tools
RUN apk update && apk add --no-cache \
    bash \
    binutils \
    coreutils \
    cryptsetup \
    curl \
    device-mapper \
    dosfstools \
    e2fsprogs \
    git \
    kmod \
    mtools \
    multipath-tools \
    openssl \
    parted \
    pmbootstrap \
    procps \
    python3 \
    py3-pip \
    shadow \
    sudo \
    tar \
    tzdata \
    util-linux \
    xz

# Setup non-root build user with passwordless sudo
RUN useradd -m -s /bin/bash builder && \
    echo "builder ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

USER builder
WORKDIR /home/builder

# Pre-configure git for pmbootstrap
RUN git config --global user.email "builder@local" && \
    git config --global user.name "Builder"

ENTRYPOINT ["/bin/bash"]
