#!/usr/bin/env bash

set -e
set -x
shopt -s dotglob

readonly name="sz3"
readonly ownership="SZ3 Upstream <kwrobot@kitware.com>"
readonly subtree="ThirdParty/$name/vtk$name"
readonly repo="https://github.com/szcompressor/SZ3.git"
readonly tag="v3.3.2"
readonly paths="
include/

.gitattributes
copyright-and-BSD-license.txt
CMakeLists.txt
README.md
SZ3Config.cmake.in
"

extract_source () {
    git_archive
}

. "${BASH_SOURCE%/*}/../update-common.sh"
