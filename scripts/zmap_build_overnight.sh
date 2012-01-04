#!/bin/bash
#
# Build zmap for all architectures specified in $ZMAP_BUILD_MACHINES.
#
# This script is run _every_ night as a cron job so be careful
# when altering it.
#
# Builds/logs are in OVERNIGHT.BUILD file/directory.
#
# Main Build Parameters:
#
# Update from repository before building   yes
#                    Version incremented    no
#                           Docs created    no
#                        Docs checked in    no
#
# Error reporting gets done by the build_run.
#

RC=0


BUILD_PREFIX='OVERNIGHT'

./build_run.sh -c -d -e -g $BUILD_PREFIX || RC=1


exit $RC
