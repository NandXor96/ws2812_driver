# -------------------------------- Base Image -------------------------------- #
# See here for image contents: https://github.com/microsoft/vscode-dev-containers/tree/v0.194.3/containers/cpp/.devcontainer/base.Dockerfile

# [Choice] Debian / Ubuntu version: debian-11, debian-10, debian-9, ubuntu-20.04, ubuntu-18.04
ARG VARIANT="ubuntu-22.04"
FROM mcr.microsoft.com/vscode/devcontainers/cpp:0-${VARIANT}

# -------------------- Frequently Used Development Tools --------------------- #

RUN export DEBIAN_FRONTEND=noninteractive && apt-get update \
    && apt-get -y install \
    silversearcher-ag \
    expect \
    doxygen \
    graphviz \
    colordiff \
    && apt-get -y autoremove \
    && apt-get -y clean \
    && rm -rf /var/lib/apt/lists/*

# Development tools typically needed for building the Linux kernel itself
RUN export DEBIAN_FRONTEND=noninteractive && apt-get update \
    && apt-get -y install \
    git \
    build-essential \
    module-assistant clang-format \
    libncurses-dev texlive-full\
    gcc-aarch64-linux-gnu unrar 7zip glances neofetch \
    binutils-aarch64-linux-gnu \
    gawk \ 
    flex iperf3 \
    bison telnet socat \
    openssl \
    libssl-dev \
    dkms mc sl cpio iputils-ping \
    libelf-dev \
    libudev-dev sphinx-common\
    libpci-dev \
    libiberty-dev \
    autoconf \
    bc \
    && apt-get -y autoremove \
    && apt-get -y clean \
    && rm -rf /var/lib/apt/lists/*

ENV NO_AT_BRIDGE=1

# Qemu-KVM, needed for testing kernels inside a VM
RUN export DEBIAN_FRONTEND=noninteractive && apt-get update \
    && apt-get -y install \
    iptables \
    libgl1-mesa-dri \
    libgl1-mesa-glx \
    libvirt-daemon-system \
    qemu-kvm \
    qemu-system-aarch64 \
    gdb-multiarch pv \
    virtinst \
    virt-viewer \
    libcanberra-gtk-module \
    libcanberra-gtk3-module \
    packagekit-gtk3-module \
    && apt-get -y autoremove \
    && apt-get -y clean \
    && rm -rf /var/lib/apt/lists/*

# Packages for Pi-Pico sdk
RUN export DEBIAN_FRONTEND=noninteractive && apt-get update \
    && apt-get -y install \
    cmake \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    libstdc++-arm-none-eabi-newlib 
    
# Packages for pyusb
RUN export DEBIAN_FRONTEND=noninteractive && apt-get update \
    && apt-get -y install \
    python3-usb

COPY ./start.sh /start.sh
COPY ./startup-scripts /opt/startup-scripts

USER vscode
RUN bash -c "echo 'echo \"Du befindest dich im Docker für das Entwickeln des WS2812 Treibers und Controllers in VSCode.\"' >> ~/.bashrc"


ENTRYPOINT ["/start.sh"]
CMD ["/bin/sh", "-c", "while sleep 1000; do :; done"]
