#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"
exec ./run-2p-legacy.sh "$@"
