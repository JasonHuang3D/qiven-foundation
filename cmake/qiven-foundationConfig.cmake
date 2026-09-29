# qiven-foundation package configuration file (F1 packaging smoke).
#
# The package is dependency-free and needs no configure-time checks:
# consuming it is importing the exported target set. All paths resolve
# relative to this file's own location, so the installed tree stays
# relocatable (the F1 install-relocation gate proves this by consuming
# the package from an arbitrary scratch prefix outside the source tree).

include("${CMAKE_CURRENT_LIST_DIR}/qiven-foundation-targets.cmake")
