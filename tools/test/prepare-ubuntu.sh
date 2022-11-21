#!/bin/sh

set -eux

local container=$1

lxc exec ${container} -- add-apt-repository ppa:mir-team/dev
lxc exec ${container} -- apt update
lxc exec ${container} -- apt install --yes \
    dpkg-dev \
    libwayland-dev \
    cmake \
    clang \
    g++ \
    pkg-config \
    libgtest-dev \
    google-mock \
    libboost-dev \
    mir-test-tools

