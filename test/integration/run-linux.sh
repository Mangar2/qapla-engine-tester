#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

cd "${repo_root}"

runner_args=(--config release)

if [[ "${QAPLA_IT_SKIP_PASSED:-0}" != "0" ]]; then
	runner_args+=(--skip-passed)
fi

while [[ $# -gt 0 ]]; do
	case "$1" in
		--binary)
			if [[ $# -lt 2 ]]; then
				echo "Missing value for --binary" >&2
				exit 2
			fi
			export QAPLA_IT_BINARY="$2"
			shift 2
			;;
		--enginesfile)
			if [[ $# -lt 2 ]]; then
				echo "Missing value for --enginesfile" >&2
				exit 2
			fi
			export QAPLA_IT_ENGINESFILE="$2"
			shift 2
			;;
		*)
			runner_args+=("$1")
			shift
			;;
	esac
done

python3 test/integration/test_runner.py "${runner_args[@]}"