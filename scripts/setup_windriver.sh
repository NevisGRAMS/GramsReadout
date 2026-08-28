#!/bin/bash
set -euo pipefail

if [ -z "${WD_BASEDIR:-}" ]; then
    echo "setup_windriver.sh: WD_BASEDIR is not set" >&2
    exit 1
fi

WDREG="${WD_BASEDIR}/redist/wdreg"
if [ ! -x "$WDREG" ]; then
    echo "setup_windriver.sh: wdreg not found or not executable: ${WDREG}" >&2
    exit 1
fi

sudo "$WDREG" windrvr1630 auto
sudo rmmod windrvr1630 && sudo modprobe windrvr1630
echo "Set up WinDriver from ${WD_BASEDIR}"
