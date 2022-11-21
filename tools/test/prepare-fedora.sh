#!/bin/sh

set -eux

lxc exec $1 -- dnf install --assumeyes \
  wayland-devel \
  cmake \
  make \
  clang \
  gcc-c++ \
  libasan.x86_64 \
  libtsan.x86_64 \
  libubsan.x86_64 \
  pkg-config \
  boost-devel \
  gtest-devel \
  gmock-devel

