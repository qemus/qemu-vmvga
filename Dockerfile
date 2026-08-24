# syntax=docker/dockerfile:1

FROM debian:trixie-slim AS builder

ARG VERSION_ARG="0.0.0"
ARG QEMU_VERSION="11.1.0"

ARG QEMU_REF="84f07211cc5b4fc6a371559bf8a5de4fb068e648"
ARG VIRGL_REF="7fcfce49616974dc7050fdbfb5bb915f4448d270"

ARG DEBIAN_FRONTEND="noninteractive"
ARG DEBIAN_SNAPSHOT="20260819T142328Z"

RUN <<EOF_BUILD_DEPS
  set -eu

  apt-get update
  apt-get install --no-install-recommends -y ca-certificates

  cat > /etc/apt/sources.list.d/qemu-snapshot.list <<EOF_SOURCES
deb [check-valid-until=no] https://snapshot.debian.org/archive/debian/${DEBIAN_SNAPSHOT}/ sid main
deb-src [check-valid-until=no] https://snapshot.debian.org/archive/debian/${DEBIAN_SNAPSHOT}/ sid main
EOF_SOURCES

  apt-get update
  apt-get build-dep --no-install-recommends -y -t sid qemu
  apt-get install --no-install-recommends -y -t sid \
    binutils \
    dpkg-dev \
    git \
    libdrm-dev \
    libepoxy-dev \
    libgbm-dev \
    libglib2.0-dev \
    libvulkan-dev \
    meson \
    ninja-build \
    pkg-config \
    python3-mako \
    python3-yaml

  rm -rf /var/lib/apt/lists/*
EOF_BUILD_DEPS

WORKDIR /src

# Helios scanout code uses virglrenderer's extended resource metadata API.
# Build against the exact virglrenderer API used by qemu-render so that support
# cannot silently compile out when Debian ships older development headers.
RUN <<EOF_VIRGL
  set -eu

  git init virglrenderer
  git -C virglrenderer remote add origin https://gitlab.freedesktop.org/virgl/virglrenderer.git
  git -C virglrenderer fetch --depth=1 origin "${VIRGL_REF}"
  git -C virglrenderer checkout --detach FETCH_HEAD

  multiarch="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"

  meson setup /build-virgl /src/virglrenderer \
    --buildtype=release \
    --prefix=/usr/local \
    --libdir="lib/${multiarch}" \
    -Dplatforms=egl \
    -Dvenus=true \
    -Drender-server-worker=thread \
    -Dunstable-apis=true \
    -Dtests=false \
    -Dvideo=false

  meson compile -C /build-virgl
  meson install -C /build-virgl
  ldconfig
EOF_VIRGL

RUN <<EOF_SOURCE
  set -eu

  git init qemu
  git -C qemu remote add origin https://gitlab.com/qemu-project/qemu.git
  git -C qemu fetch --depth=1 origin "refs/tags/v${QEMU_VERSION}"
  git -C qemu checkout --detach FETCH_HEAD

  actual="$(git -C qemu rev-parse HEAD)"
  if [ "$actual" != "${QEMU_REF}" ]; then
    echo "FAIL: QEMU v${QEMU_VERSION} resolved to $actual instead of ${QEMU_REF}."
    exit 1
  fi

  # A git tag checkout does not contain Meson wrap sources. Prefetch the
  # subprojects required by the system UI and TCG test configuration so the
  # later --disable-download configure step can remain offline.
  meson subprojects download --sourcedir qemu \
    keycodemapdb \
    berkeley-softfloat-3 \
    berkeley-testfloat-3

EOF_SOURCE

COPY files/ /tmp/helios-files/
COPY patches/ /tmp/helios-patches/

RUN <<'EOF_PATCHES'
  set -eu

  install -Dm644 /tmp/helios-files/vulkan-readback.c /src/qemu/ui/vulkan-readback.c
  install -Dm644 /tmp/helios-files/vulkan-readback.h /src/qemu/ui/vulkan-readback.h

  for patch in /tmp/helios-patches/*.patch; do
    echo "Applying ${patch##*/}..."
    git -C /src/qemu apply --recount --check "$patch"
    git -C /src/qemu apply --recount "$patch"
  done

  git -C /src/qemu diff --check
EOF_PATCHES

RUN <<'EOF_BUILD'
  set -eu

  mkdir /build /out
  cd /build

  multiarch="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
  export PKG_CONFIG_PATH="/usr/local/lib/${multiarch}/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
  export LD_LIBRARY_PATH="/usr/local/lib/${multiarch}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  export DEB_CFLAGS_MAINT_APPEND="-ffile-prefix-map=/src/qemu=."

  extra_cflags="$(dpkg-buildflags --get CFLAGS) $(dpkg-buildflags --get CPPFLAGS)"
  extra_ldflags="$(dpkg-buildflags --get LDFLAGS)"

  printf 'Debian CFLAGS/CPPFLAGS: %s\n' "$extra_cflags"
  printf 'Debian LDFLAGS: %s\n' "$extra_ldflags"

  /src/qemu/configure \
    --with-pkgversion="Helios ${VERSION_ARG}" \
    --target-list=x86_64-softmmu \
    --prefix=/usr \
    --libdir="/usr/lib/${multiarch}" \
    --libexecdir=/usr/lib/qemu \
    --sysconfdir=/etc \
    --localstatedir=/var \
    --mandir=/usr/share/man \
    --firmwarepath=/usr/share/qemu:/usr/share/seabios \
    --extra-cflags="$extra_cflags" \
    --extra-ldflags="$extra_ldflags" \
    --audio-drv-list=alsa,oss \
    --disable-blkio \
    --disable-bzip2 \
    --disable-containers \
    --disable-curl \
    --disable-docs \
    --disable-download \
    --disable-gtk \
    --disable-install-blobs \
    --disable-jack \
    --disable-libiscsi \
    --disable-libnfs \
    --disable-libssh \
    --disable-linux-user \
    --disable-modules \
    --disable-pa \
    --disable-pipewire \
    --disable-rbd \
    --disable-relocatable \
    --disable-sdl \
    --disable-sndio \
    --disable-strip \
    --disable-tools \
    --disable-user \
    --disable-vte \
    --disable-xkbcommon \
    --disable-xen \
    --enable-system \
    --enable-attr \
    --enable-bpf \
    --enable-brlapi \
    --enable-cap-ng \
    --enable-capstone \
    --enable-curses \
    --enable-fdt \
    --enable-fuse \
    --enable-gnutls \
    --enable-kvm \
    --enable-libpmem \
    --enable-libusb \
    --enable-libudev \
    --enable-linux-aio \
    --enable-linux-io-uring \
    --enable-nettle \
    --enable-numa \
    --enable-opengl \
    --enable-pixman \
    --enable-png \
    --enable-rdma \
    --enable-seccomp \
    --enable-slirp \
    --enable-smartcard \
    --enable-spice \
    --enable-tcg \
    --enable-usb-redir \
    --enable-vde \
    --enable-vhost-net \
    --enable-vhost-user \
    --enable-vhost-vdpa \
    --enable-virglrenderer \
    --enable-virtfs \
    --enable-vnc \
    --enable-vnc-jpeg \
    --enable-vnc-sasl \
    --enable-zstd

  # Print the resolved Meson configuration in the CI log. Every enabled
  # feature above is also a hard configure-time requirement, so dependencies
  # cannot disappear silently when the Debian snapshot changes.
  meson configure /build

  ninja qemu-system-x86_64

  install -Dm755 /build/qemu-system-x86_64 /out/qemu-system-x86_64
  strip --strip-unneeded /out/qemu-system-x86_64

  # This symbol is referenced only when the extended virglrenderer metadata API
  # was visible at compile time; without it native Helios scanout is incomplete.
  readelf -Ws /out/qemu-system-x86_64 \
    | grep -Fq 'virgl_renderer_resource_get_info_ext' || {
      echo "FAIL: virglrenderer extended resource metadata support was not compiled in."
      exit 1
    }

  for marker in \
    helios_scanout_bind \
    helios_scanout_read \
    helios_vulkan_capture \
    helios_vulkan_publish; do
    strings /out/qemu-system-x86_64 | grep -Fq "$marker" || {
      echo "FAIL: Helios marker is missing from the binary: $marker"
      exit 1
    }
  done
EOF_BUILD

# Test the produced executable inside the actual qemux/qemu runtime image.
# LD_BIND_NOW catches missing or incompatible shared-library symbols before the
# artifact is published.
FROM qemux/qemu:latest AS verify

COPY --from=builder /out/qemu-system-x86_64 /tmp/qemu-system-x86_64

RUN <<'EOF_VERIFY'
  set -eu

  binary=/tmp/qemu-system-x86_64

  deps="$(ldd "$binary" 2>&1)"
  printf '%s\n' "$deps"
  if printf '%s\n' "$deps" | grep -q 'not found'; then
    echo "FAIL: one or more QEMU runtime dependencies could not be resolved."
    exit 1
  fi

  LD_BIND_NOW=1 "$binary" --version \
    | grep -F "QEMU emulator version 11.1.0"

  QEMU_MODULE_DIR=/nonexistent LD_BIND_NOW=1 \
    "$binary" -device virtio-vga-gl,help \
    >/tmp/virtio-vga-gl-help 2>&1
  grep -F "host3d_blob_limit" /tmp/virtio-vga-gl-help

  QEMU_MODULE_DIR=/nonexistent LD_BIND_NOW=1 \
    "$binary" -display help >/tmp/display-help 2>&1
  grep -F "egl-headless" /tmp/display-help

  QEMU_MODULE_DIR=/nonexistent LD_BIND_NOW=1 \
    "$binary" -device qxl-vga,help >/tmp/qxl-help 2>&1

  install -Dm755 "$binary" /out/qemu-system-x86_64

  size="$(stat -c %s /out/qemu-system-x86_64)"
  echo "Verified qemu-system-x86_64 (${size} bytes)"
EOF_VERIFY

FROM scratch AS artifact

ARG VERSION_ARG="0.0.0"

LABEL org.opencontainers.image.title="Helios" \
      org.opencontainers.image.description="QEMU build with patches for accelerated Windows graphics." \
      org.opencontainers.image.version="${VERSION_ARG}"

COPY --from=verify /out/qemu-system-x86_64 /usr/bin/qemu-system-x86_64
