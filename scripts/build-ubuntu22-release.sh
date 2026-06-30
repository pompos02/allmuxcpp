#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
image="ubuntu:22.04"
build_dir="build/ubuntu22-release"
dist_dir="dist"
artifact_name="allmuxcpp-linux-x86_64-ubuntu22"

mkdir -p "${repo_root}/${dist_dir}"

docker run --rm \
  -e DEBIAN_FRONTEND=noninteractive \
  -e HOST_UID="$(id -u)" \
  -e HOST_GID="$(id -g)" \
  -v "${repo_root}:/work" \
  -w /work \
  "${image}" \
  bash -euxo pipefail -c '
    apt-get update
    apt-get install -y --no-install-recommends \
      ca-certificates \
      git \
      gnupg \
      ninja-build \
      python3-pip \
      software-properties-common

    add-apt-repository -y ppa:ubuntu-toolchain-r/test
    apt-get update
    apt-get install -y --no-install-recommends \
      g++-14 \
      libboost-dev

    python3 -m pip install --no-cache-dir "cmake>=3.25"

    cmake -S . -B "'"${build_dir}"'" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=g++-14 \
      -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

    cmake --build "'"${build_dir}"'"

    cp "'"${build_dir}"'"/allmuxcpp "'"${dist_dir}/${artifact_name}"'"
    strip "'"${dist_dir}/${artifact_name}"'" || true
    chown -R "${HOST_UID}:${HOST_GID}" "'"${build_dir}"'" "'"${dist_dir}"'"
  '

echo "Built ${repo_root}/${dist_dir}/${artifact_name}"
