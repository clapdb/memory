#!/bin/bash

if [ -f "/etc/os-release" ]; then
  . /etc/os-release
elif [ -f "/etc/arch-release" ]; then
  export ID=arch
elif [ "$(uname)" == "Darwin" ]; then
  export ID=darwin
else
  echo "/etc/os-release missing."
  exit 1
fi

fbe_fedora_packages=(
  boost-devel
  llvm
  llvm-devel
  clang
  clang-tools-extra
  lld
  cmake
  fmt-devel
  git
  ninja-build
  xxhash-devel
  google-benchmark-devel
)

fbe_debian_packages=(
  cloc
  curl
  git
  python3
  python3-pip
  lld
  #cmake version of ubuntu is also too low.
  cmake
  ninja-build
  gcc
  g++
  llvm
  clang
  clangd
  clang-tidy
  libxxhash-dev
  libboost-dev
  libfmt-dev
  libbenchmark-dev
)

fbe_darwin_packages=(
  rapidjson
  cmake
  ninja
  fmt
  xxhash
  boost
  llvm
)

case "$ID" in
ubuntu | debian)
  apt-get update -y
  apt-get install -y "${fbe_debian_packages[@]}"
  ;;
fedora)
  dnf update -y
  dnf install -y "${fbe_fedora_packages[@]}"
  ;;
darwin)
  brew install -f "${fbe_darwin_packages[@]}"
  ;;
*)
  echo "Your system ($ID) is not supported by this script. Please install dependencies manually."
  exit 1
  ;;
esac
