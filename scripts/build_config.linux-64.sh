#!/bin/echo dot script please source


# I think I missed a trick installing the autotools
# aclocal doesn't add the system path as well as
# its install path to the search dirs
ACLOCAL_FLAGS="-I /usr/share/aclocal"


# This shouldn't get overwritten.
PKG_CONFIG=$GTK_PREFIX/bin/pkg-config
