#!/bin/sh

test_xvfb_start()
{
  dir=$1
  display_num=97
  while [ "$display_num" -lt 120 ]; do
    DISPLAY=:$display_num
    export DISPLAY
    Xvfb "$DISPLAY" -screen 0 800x600x24 -nolisten tcp >"$dir/xvfb.log" 2>&1 &
    TEST_XVFB_PID=$!
    export TEST_XVFB_PID
    n=0
    while [ "$n" -lt 40 ]; do
      if xdpyinfo -display "$DISPLAY" >/dev/null 2>&1; then
        return 0
      fi
      kill -0 "$TEST_XVFB_PID" 2>/dev/null || break
      n=$((n + 1))
      sleep 0.05
    done
    kill "$TEST_XVFB_PID" 2>/dev/null || true
    wait "$TEST_XVFB_PID" 2>/dev/null || true
    display_num=$((display_num + 1))
  done
  return 1
}

test_xvfb_stop()
{
  [ -n "${TEST_XVFB_PID:-}" ] || return 0
  kill "$TEST_XVFB_PID" 2>/dev/null || true
  wait "$TEST_XVFB_PID" 2>/dev/null || true
  TEST_XVFB_PID=
}
