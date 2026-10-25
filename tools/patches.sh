#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 ignis32
# MIDIMON's changes to its platform, kept as patches on the pinned submodules (patches/README.md):
#   patches/felucca-bsp/*.patch   applied in fm1-base/bsp/ (felucca-bsp, fm1-base's own submodule)
#   patches/fm1-base/*.patch      applied in fm1-base/
# each repository's in name order, as a series (a patch may build on an earlier one). Nothing is ever committed
# inside fm1-base/: the patches are the record. What is applied is remembered in each submodule's git directory
# (midimon-patches/: a copy of each patch applied, in order), so the series can be taken out exactly as it went in.
#   tools/patches.sh apply      the series as patches/ has it now (build.sh does, before every build): nothing to do
#                               when it is already in; else the applied one out and the new one in. A patch that
#                               does not fit (an upstream update moved its lines) stops with its name
#   tools/patches.sh status     each patch: applied, changed (patches/ differs from what is applied), or not applied
#   tools/patches.sh revert     the series out again, the last first (before bumping a submodule)
#   tools/patches.sh refresh P  write P (a patch file) again from its repository's working tree, against upstream plus
#                               the patches before P, its header kept (the patches after P out meanwhile, then in)
set -eu
cd "$(dirname "$0")/.."
GIT="git -c safe.directory=*"
ROOT=$(pwd)
[ -f fm1-base/tools/build.py ] || { echo "patches: no fm1-base/ (git submodule update --init --recursive)" >&2; exit 1; }

REPOS="fm1-base/bsp fm1-base"                       # felucca-bsp's series first
series() {                                          # series REPO: its patches, in order
    case $1 in
        fm1-base/bsp) ls patches/felucca-bsp/*.patch 2>/dev/null || true ;;
        fm1-base) ls patches/fm1-base/*.patch 2>/dev/null || true ;;
    esac
}
repo_of() {
    case $1 in
        patches/felucca-bsp/*) echo fm1-base/bsp ;;
        patches/fm1-base/*) echo fm1-base ;;
        *) echo "patches: $1 is not under patches/felucca-bsp or patches/fm1-base" >&2; exit 2 ;;
    esac
}
state_dir() { echo "$($GIT -C "$1" rev-parse --absolute-git-dir)/midimon-patches"; }
applied() { ls "$(state_dir "$1")"/*.patch 2>/dev/null || true; }   # the copies, in order

take_out() {                                        # take_out REPO: the applied series out, the last first
    for c in $(applied "$1" | sort -r); do
        $GIT -C "$1" apply -R "$c"
        rm -f "$c"
        echo "patches: took out $(basename "$c") ($1)"
    done
}
put_in() {                                          # put_in REPO PATCH: in, and remembered
    s=$(state_dir "$1")
    mkdir -p "$s"
    if ! $GIT -C "$1" apply --whitespace=nowarn "$ROOT/$2"; then
        echo "patches: $2 does not fit $1 (an upstream update?): see patches/README.md" >&2
        exit 1
    fi
    cp "$2" "$s/$(basename "$2")"
    echo "patches: applied $2"
}
in_place() {                                        # in_place REPO: the applied series is the one in patches/
    want=$(series "$1" | while read -r p; do basename "$p"; done)
    have=$(applied "$1" | while read -r c; do basename "$c"; done)
    [ "$want" = "$have" ] || return 1
    for p in $(series "$1"); do cmp -s "$p" "$(state_dir "$1")/$(basename "$p")" || return 1; done
}

case ${1:-status} in
    status)
        for r in $REPOS; do
            for p in $(series "$r"); do
                c="$(state_dir "$r")/$(basename "$p")"
                if [ ! -f "$c" ]; then st="open"; elif cmp -s "$p" "$c"; then st="applied"; else st="changed"; fi
                printf '%-9s %s\n' "$st" "$p"
            done
        done ;;
    apply)
        for r in $REPOS; do
            in_place "$r" && continue
            take_out "$r"
            for p in $(series "$r"); do put_in "$r" "$p"; done
        done ;;
    revert)
        for r in fm1-base fm1-base/bsp; do take_out "$r"; done ;;
    refresh)
        p=${2:?tools/patches.sh refresh PATCH}
        r=$(repo_of "$p")
        before=$(series "$r" | awk -v p="$p" '$0 < p')
        after=$(series "$r" | awk -v p="$p" '$0 > p')
        for o in $(echo "$after" | sort -r); do         # the patches after P out of the tree (as applied)
            c="$(state_dir "$r")/$(basename "$o")"
            [ -f "$c" ] && { $GIT -C "$r" apply -R "$c"; rm -f "$c"; }
        done
        scratch=$(mktemp -d)                            # upstream plus the patches before P, in a scratch repository
        $GIT -C "$r" archive HEAD | tar -x -C "$scratch"
        G="git -C $scratch -c user.name=patches -c user.email=patches@localhost"
        $G init -q && $G add -A && $G commit -qm upstream
        for o in $before; do $G apply "$ROOT/$o"; done
        $G add -A && $G commit -qm before --allow-empty
        $GIT -C "$r" ls-files -z --cached --others --exclude-standard | (cd "$r" &&   # (a submodule: not the repo's)
            xargs -0 sh -c 'd=$1; shift; for f; do [ -d "$f" ] || cp --parents "$f" "$d"; done' _ "$scratch")
        tmp=$(mktemp)
        [ -f "$p" ] && sed '/^diff --git /,$d' "$p" > "$tmp"
        $G add -A && $G diff --cached --no-color >> "$tmp"
        rm -rf "$scratch"
        mkdir -p "$(dirname "$p")" && mv "$tmp" "$p"
        mkdir -p "$(state_dir "$r")" && cp "$p" "$(state_dir "$r")/$(basename "$p")"   # (it is in the tree as written)
        for o in $after; do put_in "$r" "$o"; done
        echo "patches: $p from $r ($(grep -c '^diff --git' "$p") files)" ;;
    *) sed -n '2,17p' "$0"; exit 2 ;;
esac
