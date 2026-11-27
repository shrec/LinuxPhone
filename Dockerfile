FROM debian:trixie
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential gcc-aarch64-linux-gnu bc bison flex libssl-dev kmod cpio zstd \
      git ca-certificates curl python3 librsvg2-bin netpbm fonts-jetbrains-mono \
      mmdebstrap arch-test debian-archive-keyring e2fsprogs mkbootimg android-sdk-libsparse-utils \
    && rm -rf /var/lib/apt/lists/*
RUN sed -i 's/^Components: main$/Components: main non-free-firmware/' /etc/apt/sources.list.d/debian.sources && apt-get update
