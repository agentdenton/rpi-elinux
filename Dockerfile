FROM ubuntu:22.04

ARG USERNAME
ARG WORKDIR

ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US:en
ENV LC_ALL=en_US.UTF-8

# Set non-interactive frontend for apt-get to skip any user confirmations
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get clean && apt-get update && apt-get install -y \
    locales \
    vim \
    sudo \
    make \
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
    tmux \
    bmap-tools \
    mtools \
    parted \
    kas \
    openssh-server

RUN locale-gen en_US.UTF-8

RUN useradd --shell=/bin/bash --create-home $USERNAME
RUN echo "$USERNAME ALL=(ALL) NOPASSWD: ALL" | tee -a /etc/sudoers

RUN mkdir -p $WORKDIR
RUN mkdir -p /run/sshd
RUN mkdir -p /home/$USERNAME/.ssh

COPY scripts/startup.sh /usr/local/bin
COPY configs/ssh_config /home/$USERNAME/.ssh/config

RUN ssh-keygen -A -v

RUN sed -i 's/%USERNAME%/'"$USERNAME"'/g' /home/$USERNAME/.ssh/config

RUN chown -R $USERNAME:$USERNAME /run
RUN chown -R $USERNAME:$USERNAME /etc/ssh
RUN chown -R $USERNAME:$USERNAME /home/$USERNAME/.ssh
RUN chown $USERNAME:$USERNAME /usr/local/bin/startup.sh

USER $USERNAME

WORKDIR $WORKDIR

CMD ["/bin/bash"]
