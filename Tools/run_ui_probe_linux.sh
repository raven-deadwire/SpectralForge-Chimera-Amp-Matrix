#!/usr/bin/env bash
# Isolated X11 desktop for a real-peer benchmark or the native UI regressions.
# Requires xvfb, xauth, openbox. Neither hidden/headless/no-paint results are CPU evidence.
set -euo pipefail
exec xvfb-run -a -s '-screen 0 2560x1600x24' bash -c '
  openbox --sm-disable >/tmp/chimera-ui-openbox.log 2>&1 &
  chimera_wm_pid=$!
  trap '\''kill "$chimera_wm_pid" 2>/dev/null || true'\'' EXIT
  sleep 1
  "$@"
' chimera-ui "$@"
