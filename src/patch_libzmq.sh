#!/bin/sh

cd "$(dirname $0)"

if [ ! -f libzmq/autogen.sh ]; then
  # the tarball has the submodules, a fresh clone does not
  git submodule update --init --recursive
fi

cd libzmq

if [ ! -f src/Makefile.am.orig ]; then
  # remove code format helper and valgrind support that CRAN complains about
  # sed -i does not work on macOS
  mv Makefile.am Makefile.am.orig
  sed '/WITH_CLANG_FORMAT/,/VALGRIND_SUPPRESSIONS_FILES/d' Makefile.am.orig > Makefile.am
fi

if [ ! -f src/curve_client_tools.hpp.orig ]; then
  # remove disabled gcc check that cran complains about
  mv src/curve_client_tools.hpp src/curve_client_tools.hpp.orig
  sed '/^#pragma/s|^|//|' src/curve_client_tools.hpp.orig > src/curve_client_tools.hpp
fi
if [ ! -f include/zmq_utils.h.orig ]; then
  mv include/zmq_utils.h include/zmq_utils.h.orig
  sed '/^#pragma/s|^|//|' include/zmq_utils.h.orig > include/zmq_utils.h
fi

if [ ! -f Makefile.in ]; then
  ./autogen.sh || exit 1
fi
