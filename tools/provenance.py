#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal
"""Where a project's lines came from in Felucca: the check for moving a Felucca-based firmware onto felucca-bsp and
fm1-base, for a firmware whose author relicenses it to MPL-2.0 (fm1-base's skill/felucca-to-fm1-base, RULES.md).

  tools/provenance.py PROJECT FELUCCA [--allow DIR ...] [--rev REV] [--prs 2,8,...] [--min 12] [--files GLOB]
                      [--lines]

PROJECT  a directory (its git-tracked files, or every file if it is not a repository)
FELUCCA  a checkout of https://github.com/hugelton/Felucca
--allow  an MPL-2.0 tree whose lines may be in PROJECT though they are also Felucca's: felucca-bsp and fm1-base
         (repeat it; default: this fm1-base and its bsp/)

A firmware moved to MPL-2.0 takes nothing from Felucca except what felucca-bsp and fm1-base already carry under
MPL-2.0 (Leo approved his code there, felucca-bsp#4; keremimo his part of usb.c, felucca-bsp#3): everything else
from Felucca, Leo's own code included, is replaced by fm1-base's or rewritten. This lists, file by file, PROJECT's
lines that are Felucca's and in no --allow tree, and whose they are.

Each line is looked up among Felucca's lines at REV (default HEAD). Felucca's `git blame` names who committed it,
and the lines added by the pull requests in --prs (default: every "#N" in Felucca's CREDITS.md, fetched with
`gh pr diff` and kept in FELUCCA/.provenance/) are the pull request's author's, whoever committed them (Leo applied
many under his own name). A line already in the code a pull request changed (among its removed or context lines)
is not its author's; one that is in Felucca twice may be a pull request's copy of existing code ("ambiguous").
A copy shows as three or more of Felucca's lines in a row; one or two alone are most likely common lines (a loop, a
call) and are only counted ("single lines"). Lines shorter than --min characters (after the spaces), in three or
more of Felucca's files, or from licence texts and headers, say nothing and are left out. --lines prints the lines found.

It finds copies, not rewrites: code renamed or restructured beyond the line level, or translated, is not found, so
a clean report is necessary, not sufficient. Third-party code in Felucca (engines under MIT or Apache-2.0, fonts,
samples) keeps its own licence: see Felucca's LICENSING.md and CREDITS.md."""
import argparse
import collections
import fnmatch
import os
import re
import subprocess
import sys

EXT = (".c", ".h", ".S", ".s", ".ld", ".py", ".js", ".mjs", ".ts", ".html", ".css", ".md", ".sh", ".toml", ".txt",
       ".json", ".cpp", ".hpp", ".cc")
LEO = {"kurogedelic", "leo kuroshita", "kurogedelic@gmail.com"}
RUN = 3                                           # Felucca's lines in a row that look like a copy
COMMON = 3                                        # a line in this many of Felucca's files says nothing


def run(args, cwd=None, check=True):
    r = subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors="replace")
    if check and r.returncode != 0:
        sys.exit(f"provenance: {' '.join(args)}: {r.stderr.strip()}")
    return r.stdout


def norm(line):
    return re.sub(r"\s+", " ", line.strip())


LICENCE_WORDS = re.compile(r"licen[cs]e|warrant|copyright|spdx|permission is hereby|redistribut|merchantab|"
                           r"apache|gnu general|mozilla public|express or implied|limitations under", re.I)


def meaningful(n, minlen):
    return len(n) >= minlen and re.search(r"[A-Za-z0-9]{3}", n) and not LICENCE_WORDS.search(n)


def is_leo(who):
    return who.lower() in LEO


def felucca_lines(fel, rev, minlen):
    """normalised line -> Counter of committers (at rev), and line -> set of files (for the common-line filter)"""
    owners, where = collections.defaultdict(collections.Counter), collections.defaultdict(set)
    for path in run(["git", "ls-tree", "-r", "--name-only", rev], cwd=fel).split("\n"):
        if not path.endswith(EXT):
            continue
        out = run(["git", "blame", "--line-porcelain", rev, "--", path], cwd=fel, check=False)
        who = None
        for ln in out.split("\n"):
            if ln.startswith("author "):
                who = ln[7:].strip()
            elif ln.startswith("\t"):
                n = norm(ln[1:])
                if meaningful(n, minlen):
                    owners[n][who] += 1
                    where[n].add(path)
    return owners, where


def pr_lines(fel, prs, minlen):
    """normalised line -> set of "login #N" for each pull request's added lines"""
    cache = os.path.join(fel, ".provenance")
    os.makedirs(cache, exist_ok=True)
    out = collections.defaultdict(set)
    for n in prs:
        f = os.path.join(cache, f"pr{n}.txt")
        if not os.path.exists(f):
            login = run(["gh", "pr", "view", str(n), "-R", "hugelton/Felucca", "--json", "author", "--jq",
                         ".author.login"], check=False).strip() or "?"
            diff = run(["gh", "pr", "diff", str(n), "-R", "hugelton/Felucca"], check=False)
            with open(f, "w") as fh:
                fh.write(login + "\n" + diff)
        with open(f) as fh:
            login = fh.readline().strip()
            body = fh.read().split("\n")
        before = {norm(ln[1:]) for ln in body if (ln.startswith("-") and not ln.startswith("---")) or ln.startswith(" ")}
        for ln in body:
            if ln.startswith("+") and not ln.startswith("+++"):
                nl = norm(ln[1:])
                if meaningful(nl, minlen) and nl not in before:    # (already there, removed or as context: not written)
                    out[nl].add(f"{login} #{n}")
    return out


def credited_prs(fel):
    p = os.path.join(fel, "CREDITS.md")
    if not os.path.exists(p):
        return []
    return sorted({int(x) for x in re.findall(r"#(\d+)", open(p, errors="replace").read())})


def project_files(proj, glob):
    r = subprocess.run(["git", "ls-files"], cwd=proj, capture_output=True, text=True)
    files = r.stdout.split("\n") if r.returncode == 0 else [
        os.path.relpath(os.path.join(d, f), proj) for d, _, fs in os.walk(proj) for f in fs]
    return sorted(f for f in files if f.endswith(EXT) and (not glob or fnmatch.fnmatch(f, glob))
                  and os.path.isfile(os.path.join(proj, f)))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("project")
    ap.add_argument("felucca")
    ap.add_argument("--allow", action="append", help="an MPL-2.0 tree whose lines may be in PROJECT")
    ap.add_argument("--rev", default="HEAD")
    ap.add_argument("--prs", help="pull request numbers, comma-separated (default: CREDITS.md's)")
    ap.add_argument("--min", type=int, default=12, help="shortest line that counts (characters, default 12)")
    ap.add_argument("--files", help="only these files (a glob, e.g. 'device/*')")
    ap.add_argument("--lines", action="store_true", help="print the lines found in runs")
    a = ap.parse_args()
    prs = [int(x) for x in a.prs.split(",")] if a.prs else credited_prs(a.felucca)
    print(f"Felucca at {run(['git', 'rev-parse', '--short', a.rev], cwd=a.felucca).strip()}; "
          f"pull requests: {', '.join('#%d' % n for n in prs) or 'none'}")
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    allow_dirs = a.allow or [here]                # fm1-base, and with it its bsp/ (felucca-bsp)
    allowed = set()
    for d in allow_dirs:
        for sub in (d, os.path.join(d, "bsp")):
            if os.path.isdir(sub):
                for f in project_files(sub, None):
                    for ln in open(os.path.join(sub, f), errors="replace"):
                        allowed.add(norm(ln))
    owners, where = felucca_lines(a.felucca, a.rev, a.min)
    from_pr = pr_lines(a.felucca, prs, a.min)
    common = {n for n, w in where.items() if len(w) >= COMMON}
    for path in run(["git", "ls-tree", "-r", "--name-only", a.rev], cwd=a.felucca).split("\n"):
        if re.match(r"(LICEN[CS]E|COPYING|LICENSES/)", path):   # licence texts: every project has them
            for ln in run(["git", "show", f"{a.rev}:{path}"], cwd=a.felucca, check=False).split("\n"):
                common.add(norm(ln))
    flagged = 0
    for f in project_files(a.project, a.files):
        counted, found, whose, hits, seq = 0, 0, collections.Counter(), [], []
        singles = 0
        for ln in open(os.path.join(a.project, f), errors="replace"):
            n = norm(ln)
            if not meaningful(n, a.min) or n in common:
                continue
            counted += 1
            if n in allowed:                      # felucca-bsp's or fm1-base's MPL-2.0 copy
                seq.append((n, None))
                continue
            if n in from_pr and sum(owners[n].values()) >= 2:
                seq.append((n, ("(ambiguous)",)))   # in Felucca twice: a pull request copying existing code, or not
            elif n in from_pr:                    # a contributor's pull request: theirs, whoever committed it
                seq.append((n, tuple(sorted(from_pr[n]))))
            elif n in owners:
                who = owners[n].most_common(1)[0][0]
                seq.append((n, ("Leo",) if is_leo(who) else (who,)))
            else:
                seq.append((n, None))
        # what a copy looks like: RUN or more of Felucca's lines in a row; one or two alone are most likely common
        # lines (a loop, a csync, a call) and are counted apart
        runs, singles = 0, 0
        i = 0
        while i < len(seq):
            if seq[i][1] is None:
                i += 1
                continue
            j = i
            while j < len(seq) and seq[j][1] is not None:
                j += 1
            for k in range(i, j):
                n, who = seq[k]
                if j - i >= RUN:
                    runs += 1
                    for w in who:
                        whose[w] += 1
                    hits.append((n, list(who)))
                else:
                    singles += 1
            i = j
        found = runs
        if not found:
            continue
        flagged += 1
        parts = ", ".join(f"{w} {c}" for w, c in whose.most_common())
        print(f"{f}: {found} of {counted} lines are Felucca's, in runs, and not in felucca-bsp or fm1-base ({parts}"
              f"{f'; and {singles} single lines' if singles else ''}): replace or rewrite")
        if a.lines:
            for n, who in hits:
                print(f"    [{', '.join(who)}] {n[:110]}")
    print(f"{flagged} file(s) with Felucca's lines outside felucca-bsp and fm1-base")


if __name__ == "__main__":
    main()
