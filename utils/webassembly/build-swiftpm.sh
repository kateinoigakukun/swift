#!/bin/bash
set -e
DESTINATION_TOOLCHAIN=$1
SOURCE_PATH="$(cd "$(dirname $0)/../../.." && pwd)"
NIGHTLY_TOOLCHAIN=${SOURCE_PATH}/swift-nightly-toolchain

export PATH=$NIGHTLY_TOOLCHAIN/usr/bin:$PATH

SWIFT_BUILD_FLAGS="-c release"
SWIFT_BUILD=${NIGHTLY_TOOLCHAIN}/usr/bin/swift-build

build_swiftpm() {
  cd ${SOURCE_PATH}/swiftpm
  ${SWIFT_BUILD} ${SWIFT_BUILD_FLAGS}
}

install_binary() {
  local src=$1
  local dest=${DESTINATION_TOOLCHAIN}/usr/bin
  echo "Installing ${src} to ${dest}"
  cp ${src} ${dest}
}

install_runtime_file() {
  local src=$1
  local dest=${DESTINATION_TOOLCHAIN}/usr/lib/swift/pm
  echo "Installing ${src} to ${dest}/{4,4_2}"
  for runtime in "4" "4_2"
  do
    mkdir -p ${dest}/${runtime}
    cp ${src} ${dest}/${runtime}
  done
}

install_swiftpm() {
  cd ${SOURCE_PATH}/swiftpm
  local bin_path=$(${SWIFT_BUILD} ${SWIFT_BUILD_FLAGS} --show-bin-path)
  for binary in ${bin_path}/{swift-build,swift-test,swift-run,swift-package}
  do
    install_binary ${binary}
  done

  if [[ "$(uname)" == "Linux" ]]; then
    install_runtime_file "${bin_path}/libPackageDescription.so"
    install_runtime_file "${bin_path}/PackageDescription.swiftmodule"
    install_runtime_file "${bin_path}/PackageDescription.swiftdoc"
  else
    install_runtime_file "${bin_path}/libPackageDescription.dylib"
    install_runtime_file "${bin_path}/PackageDescription.swiftinterface"
  fi
}

build_swiftpm
install_swiftpm
