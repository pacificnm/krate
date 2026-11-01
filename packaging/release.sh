#!/bin/sh
# Make a release (docs/UPDATES.md#releases): packaging/release.sh 0.3.0
# Sets the version in CMakeLists.txt, commits it, tags v0.3.0 and pushes
# both; GitHub then builds, tests and publishes the release, and every
# Krate offers it under UPDATE.
set -eu
cd "$(dirname "$0")/.."

version=${1:-}
echo "$version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' || { echo "usage: $0 <major.minor.patch>, e.g. 0.3.0" >&2; exit 1; }
[ "$(git rev-parse --abbrev-ref HEAD)" = main ] || { echo "release from main" >&2; exit 1; }
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo "commit or stash your changes first" >&2; exit 1; }
git fetch -q origin
[ "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)" ] || { echo "main isn't the same as origin/main: pull or push first" >&2; exit 1; }
! git rev-parse -q --verify "refs/tags/v$version" >/dev/null || { echo "v$version is taken" >&2; exit 1; }
current=$(sed -n 's/^project(krate VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
# After the current number, or the current one itself if it was never released (its tag is free: checked above).
if [ "$version" != "$current" ] && [ "$(printf '%s\n%s\n' "$current" "$version" | sort -V | tail -n1)" != "$version" ]; then
    echo "$version isn't after $current" >&2
    exit 1
fi

sed -i "s/^project(krate VERSION [0-9.]*/project(krate VERSION $version/" CMakeLists.txt
git add CMakeLists.txt
git diff --cached --quiet || git commit -q -m "Release $version"
git tag -a "v$version" -m "Krate $version"
git push -q origin main "v$version"
echo "Pushed v$version: GitHub builds and publishes it (https://github.com/pacificnm/krate/actions)."
