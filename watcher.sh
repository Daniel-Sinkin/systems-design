#!/usr/bin/env bash
set -euo pipefail

watchexec -q -w src/ -- bash -c 'cmake --build build && clear && ./build/main'
