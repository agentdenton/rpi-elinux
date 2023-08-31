FROM ubuntu:22.04

ARG USERNAME

# Set non-interactive frontend for apt-get to skip any user confirmations
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get clean && apt-get update && apt-get install -y \
    locales \
    vim \
    sudo \
    make \
    help2man \
    unzip \
    git \
    wget \
    subversion \
    cmake \
    flex \
    bison \
    gperf \
    python3 \
    python3-pip \
    python3-venv \
    python3-pexpect \
    python3-git \
    python3-jinja2 \
    python3-subunit \
    ninja-build \
    ccache \
    libffi-dev \
    libssl-dev \
    dfu-util \
    libusb-1.0-0 \
    usbutils \
    sed \
    binutils \
    build-essential \
    diffutils \
    gcc \
    g++ \
    bash \
    patch \
    gzip \
    bzip2 \
    perl \
    tar \
    cpio \
    rsync \
    file \
    bc \
    findutils \
    libncurses5-dev \
    meson \
    gawk \
    diffstat \
    texinfo \
    chrpath \
    socat \
    xz-utils \
    debianutils \
    iputils-ping \
    libegl1-mesa \
    libsdl1.2-dev \
    mesa-common-dev \
    zstd \
    liblz4-tool \
    bmap-tools

RUN useradd --shell=/bin/bash --create-home $USERNAME
RUN echo "$USERNAME ALL=(ALL) NOPASSWD: ALL" | tee -a /etc/sudoers

RUN locale-gen en_US.UTF-8
ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US:en
ENV LC_ALL=en_US.UTF-8

ENV RPI_ELINUX_ROOT="/home/$USERNAME/rpi-elinux"

# Possible values: rpi0w rpi3b ...
ENV RPI_BOARD_NAME="rpi0w"

ENV BR2_EXTERNAL="/home/$USERNAME/rpi-elinux/br/br2-external"

ENV BB_ENV_PASSTHROUGH_ADDITIONS="DL_DIR SSTATE_DIR TEMPLATECONF RPI_ELINUX_ROOT"
ENV SSTATE_DIR="$RPI_ELINUX_ROOT/yocto/cache/sstate-cache"
ENV DL_DIR="$RPI_ELINUX_ROOT/yocto/cache/downloads"
ENV TEMPLATECONF="$RPI_ELINUX_ROOT/yocto/sources/meta-raspberrypi-custom/conf/templates/$RPI_BOARD_NAME-template"

# Enable man pages and help messages
RUN yes | unminimize

USER $USERNAME
WORKDIR /home/$USERNAME

RUN mkdir -p rpi-elinux
