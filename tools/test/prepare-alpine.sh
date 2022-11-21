#!/bin/sh

set -eux

lxc exec $1 -- apk add\
  coreutils \
  make \
  g++ \
  wayland-dev \
  cmake \
  boost-dev \
  gtest-dev
