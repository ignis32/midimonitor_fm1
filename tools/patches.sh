#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 ignis32
# MIDIMON's changes to its platform, kept as patches on the pinned submodules (patches/README.md):
#   patches/felucca-bsp/*.patch   applied in fm1-base/bsp/ (felucca-bsp, fm1-base's own submodule)
#   patches/fm1-base/*.patch      applied in fm1-base/
# in name order, felucca-bsp's first. Nothing is ever committed inside fm1-base/: the patches are the record.
#   tools/patches.sh apply      apply those not applied yet (build.sh does, before every build); one that fits
#                               neither way (an upstream update moved its lines) stops with its name
#   tools/patches.sh status     each patch: applied, not applied, or does not fit
#   tools/patches.sh revert     take them all out again, the last first (before bumping a submodule)
#   tools/patches.sh refresh P  write P (a patch file) again from its repository's working tree, its header kept:
#                               the other patches on that repository taken out meanwhile, then put back
set -eu
cd "$(dirname "$0")/.."
GIT="git -c safe.directory=*"

dir_of() {                                  # the patch's repository
    case $1 in
        patches/felucca-bsp/*) echo fm1-base/bsp ;;
        patches/fm1-base/*) echo fm1-base ;;
        *) echo "patches: $1 is not under patches/felucca-bsp or patches/fm1-base" >&2; exit 2 ;;
    esac
}
list() { for d in felucca-bsp fm1-base; do ls patches/$d/*.patch 2>/dev/null || true; done; }
state() {                                   # state PATCH: applied | open | conflict
    d=$(dir_of "$1")
    if $GIT -C "$d" apply --check -R "$ROOT/$1" 2>/dev/null; then echo applied
    elif $GIT -C "$d" apply --check "$ROOT/$1" 2>/dev/null; then echo open
    else echo conflict; fi
}
ROOT=$(pwd)
[ -f fm1-base/tools/build.py ] || { echo "patches: no fm1-base/ (git submodule update --init --recursive)" >&2; exit 1; }

case ${1:-status} in
    status)
        for p in $(list); do printf '%-9s %s\n' "$(state "$p")" "$p"; done ;;
    apply)
        for p in $(list); do
            case $(state "$p") in
                applied) ;;
                open) $GIT -C "$(dir_of "$p")" apply --whitespace=nowarn "$ROOT/$p"; echo "patches: applied $p" ;;
                conflict) echo "patches: $p does not fit $(dir_of "$p") (an upstream update?): see patches/README.md" >&2; exit 1 ;;
            esac
        done ;;
    revert)
        for p in $(list | sort -r); do
            [ "$(state "$p")" = applied ] || continue
            $GIT -C "$(dir_of "$p")" apply -R "$ROOT/$p"; echo "patches: reverted $p"
        done ;;
    refresh)
        p=${2:?tools/patches.sh refresh PATCH}
        d=$(dir_of "$p")
        others=$(list | grep -v -x "$p" | while read -r o; do if [ "$(dir_of "$o")" = "$d" ]; then echo "$o"; fi; done)
        for o in $(echo "$others" | sort -r); do $GIT -C "$d" apply -R "$ROOT/$o"; done
        tmp=$(mktemp)
        [ -f "$p" ] && sed '/^diff --git /,$d' "$p" > "$tmp"
        $GIT -C "$d" diff --no-color --ignore-submodules=all >> "$tmp"   # (new files: git -C REPO add -N FILE first)
        for o in $others; do $GIT -C "$d" apply "$ROOT/$o"; done
        mkdir -p "$(dirname "$p")" && mv "$tmp" "$p"
        echo "patches: $p from $d ($(grep -c '^diff --git' "$p") files)" ;;
    *) sed -n '2,15p' "$0"; exit 2 ;;
esac
