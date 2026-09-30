#!/bin/sh
set -eu

case ${1:-value} in
value)
  printf '%s\n' work
  ;;
empty)
  printf '\n'
  ;;
cancel)
  exit 1
  ;;
*)
  exit 2
  ;;
esac
