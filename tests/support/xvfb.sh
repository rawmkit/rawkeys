#!/bin/sh

# Private Xvfb lifecycle for rawkeys integration tests.
#
# Each test atomically claims one display number before starting Xvfb so
# parallel Meson runs cannot race each other or an unrelated local X server.
# Readiness is checked with a tiny Xlib probe built by this repository rather
# than an external X11 utility.

test_xvfb_pid=
test_xvfb_lock=
test_xvfb_display=

test_xvfb_stop()
{
  if [ -n "$test_xvfb_pid" ]; then
    kill "$test_xvfb_pid" 2>/dev/null || true
    wait "$test_xvfb_pid" 2>/dev/null || true
    test_xvfb_pid=
  fi

  if [ -n "$test_xvfb_lock" ]; then
    rmdir "$test_xvfb_lock" 2>/dev/null || true
    test_xvfb_lock=
  fi

  test_xvfb_display=
}

test_xvfb_start()
{
  state_dir=$1
  display_probe=$2
  lock_root=${TMPDIR:-/tmp}/rawkeys-xvfb-displays

  mkdir -p "$lock_root" || return 1

  display_number=90
  while [ "$display_number" -lt 190 ]; do
    candidate_lock=$lock_root/$display_number
    if mkdir "$candidate_lock" 2>/dev/null; then
      if [ -e "/tmp/.X${display_number}-lock" ] ||
         [ -e "/tmp/.X11-unix/X${display_number}" ]; then
        rmdir "$candidate_lock" 2>/dev/null || true
        display_number=$((display_number + 1))
        continue
      fi

      candidate_display=:$display_number
      log=$state_dir/xvfb.log
      Xvfb "$candidate_display" -screen 0 800x600x24 -nolisten tcp >"$log" 2>&1 &
      candidate_pid=$!

      tries=0
      while [ "$tries" -lt 100 ]; do
        if DISPLAY=$candidate_display "$display_probe" >/dev/null 2>&1; then
          test_xvfb_pid=$candidate_pid
          test_xvfb_lock=$candidate_lock
          test_xvfb_display=$candidate_display
          DISPLAY=$candidate_display
          export DISPLAY
          return 0
        fi

        if ! kill -0 "$candidate_pid" 2>/dev/null; then
          break
        fi

        tries=$((tries + 1))
        sleep 0.02
      done

      if kill -0 "$candidate_pid" 2>/dev/null; then
        kill "$candidate_pid" 2>/dev/null || true
      fi
      wait "$candidate_pid" 2>/dev/null || true
      rmdir "$candidate_lock" 2>/dev/null || true

      echo "rawkeys tests: Xvfb startup failed" >&2
      cat "$log" >&2 || true
      return 1
    fi

    display_number=$((display_number + 1))
  done

  echo "rawkeys tests: no free Xvfb display in :90..:189" >&2
  return 1
}
