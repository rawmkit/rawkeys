#!/bin/sh
set -eu
case $0 in */*) test_dir=${0%/*} ;; *) test_dir=. ;; esac
. "$test_dir/support/xvfb.sh"

test_bin=$1
display_probe=$2
tmp=${TMPDIR:-/tmp}/rawkeys-rawm-clients.$$
mkdir -p "$tmp"
cleanup()
{
  test_xvfb_stop
  rm -rf "$tmp"
}
trap cleanup EXIT HUP INT TERM
command -v Xvfb >/dev/null 2>&1 || { echo "rawkeys rawm clients: skipped (Xvfb unavailable)"; exit 77; }
test_xvfb_start "$tmp" "$display_probe" || { echo "rawkeys rawm clients: skipped (Xvfb unusable)"; exit 77; }
"$test_bin"
