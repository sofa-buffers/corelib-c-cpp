---
name: release
description: Cut a release of corelib-c-cpp — bump the four version manifests, open the release PR, tag main and publish the GitHub release. Use when the user asks to release, cut a version, bump the version, or tag a new version of this library.
---

# Release corelib-c-cpp

The **git tag is the source of truth** for the version. Everything else — four
package manifests — is brought into line with it, and a CI workflow enforces
that they match once the tag exists.

`.github/workflows/version-consistency.yaml` has two halves. The
**cross-manifest** half — all four manifests agree, every name and license
matches `conanfile.py` — runs on pull requests and on `main`, so it blocks a
mistake before the tag. The **tag** half — the agreed version equals the tag,
and the tag is well-formed, annotated and on `main` — runs on `v*` tags only.

It was tags-only, and that is how `v0.9.0` shipped with `library.properties`
still saying `0.8.0` (`c590a55`): nothing checked the release PR. Step 4 below is
now a fast local pre-check rather than the only thing standing between you and a
bad tag — but run it anyway, because a red release PR costs a round trip and a
bad tag costs a re-tag.

## 1. Preconditions

```bash
cd /workspace
git checkout main && git pull -p
git status --porcelain          # must be empty
gh run list --branch main --limit 5   # CI green on the commit you are releasing
git tag --sort=-v:refname | head -3   # newest tag = the previous release
```

Do not release from a dirty tree or a red main.

## 2. Choose the version

Pre-1.0 rule for this family: **a minor bump may break API or wire output.**
A patch bump may not. Decide from what landed since the previous tag:

```bash
git log --oneline "$(git describe --tags --abbrev=0)"..main
```

Versions are **aligned across the SofaBuffers family** (corelib, sofabgen, the
other language corelibs release at the same number). Confirm the number with the
user rather than inferring it from this repo alone.

Note the manifests may already sit **ahead** of the newest tag — a feature PR
sometimes bumps them mid-stream (e.g. `fe86011` moved them to `0.11.0`). The
version-consistency workflow deliberately tolerates that between releases, so a
manifest reading `X.Y.Z` is *not* evidence that `vX.Y.Z` was released. Check the
tags, not the manifests.

## 3. Bump all four version sites

These four, and only these four. `X.Y.Z` is the version without the `v`:

| File | Line shape |
|---|---|
| `CMakeLists.txt` | `    VERSION X.Y.Z` inside `project(sofabuffers` — **must stay on its own line**, the CI check greps for exactly that |
| `conanfile.py` | `    version = "X.Y.Z"` |
| `library.json` | `  "version": "X.Y.Z",` |
| `library.properties` | `version=X.Y.Z` |

**`library.properties` is the one that gets missed.** It has no file extension,
so a survey by glob (`*.json`, `*.toml`, …) walks straight past it. That is
literally how the 0.9.0 bug happened. Edit it explicitly, every time.

Do **not** hand-edit these — they are already correct and derive the version:

- `Doxyfile.in` — uses `@PROJECT_VERSION@`, configured from CMake
- `README.md` — badges are dynamic endpoints, no version is hardcoded
- `src/**` — there is no `SOFAB_VERSION` macro in this library
- there is no CHANGELOG in this repo

## 4. Verify locally — same comparisons the CI gate makes

```bash
cd /workspace
V=X.Y.Z   # the version you are releasing, no leading v

cmake_v=$(grep -oP '^\s*VERSION \K[0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt | head -1)
conan_v=$(grep -oP '^\s*version\s*=\s*"\K[^"]+' conanfile.py | head -1)
pio_v=$(jq -r .version library.json)
ard_v=$(grep -oP '^version=\K.*' library.properties)

for pair in "CMakeLists.txt:$cmake_v" "conanfile.py:$conan_v" "library.json:$pio_v" "library.properties:$ard_v"; do
  f=${pair%%:*}; got=${pair#*:}
  [[ "$got" == "$V" ]] && echo "ok   $f = $got" || echo "FAIL $f = '$got' != '$V'"
done

# name and license: conanfile.py is the source of truth for both
n=$(grep -oP '^\s*name\s*=\s*"\K[^"]+' conanfile.py | head -1)
l=$(grep -oP '^\s*license\s*=\s*"\K[^"]+' conanfile.py | head -1)
[[ "$(jq -r .name library.json)" == "$n" && "$(jq -r .license library.json)" == "$l" ]] \
  && echo "ok   library.json name/license" || echo "FAIL library.json name/license"
[[ "$(grep -oP '^name=\K.*' library.properties)" == "$n" && "$(grep -oP '^license=\K.*' library.properties)" == "$l" ]] \
  && echo "ok   library.properties name/license" || echo "FAIL library.properties name/license"
```

Every line must read `ok`. Also confirm the build still configures, since
`CMakeLists.txt` was touched:

```bash
cmake -S /workspace -B /tmp/rel-check >/dev/null && echo "cmake configure ok"
```

## 5. The release PR

This repo is **rebase-only**, and CI runs only on PRs based on `main`.

```bash
git checkout -b release/vX.Y.Z
git commit -am "$(cat <<'MSG'
chore(release): X.Y.Z

The git tag is the source of truth for the version; this brings every package
manifest in line with the vX.Y.Z tag that follows.

<Breaking changes since the previous tag, each naming its Crucible finding and
spec section — e.g. "Crucible F-0042 / CORELIB_PLAN §4.8 — the array-header
hook …". Under the pre-1.0 rule a minor bump may break API or wire output; say
so explicitly.>

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
MSG
)"
git push -u origin release/vX.Y.Z
gh pr create --base main --title "chore(release): X.Y.Z" --body "<same substance as the commit body>

🤖 Generated with [Claude Code](https://claude.com/claude-code)"
```

Wait for CI to go green, then merge. Beware: merging a base branch with
`--delete-branch` closes stacked PRs for good — check nothing is stacked on this one.

## 6. Tag main

**The tag name is `vX.Y.Z` — lowercase `v`, then the three-part version, nothing
else.** `v1.2.3`, never `V1.2.3`, `1.2.3` or `release-1.2.3`.

This is not cosmetic. `Version consistency` triggers on `tags: ['v*']`, and ref
patterns are case-sensitive: a tag named `V1.2.3` matches nothing, so the gate
never runs and the release *looks* verified while nothing checked the manifests.
The workflow then derives the reference version with `${GITHUB_REF_NAME#v}`,
which strips a lowercase `v` only — so a stray capital would also compare
`V1.2.3` against a manifest reading `1.2.3` and fail on every one of them.

The manifests themselves carry the bare version, with no `v` (step 3).

The gate now asserts this on every tag it sees, along with two things nothing
checked before: that the tag is **annotated**, and that it points at a commit on
`origin/main`. All three are hard errors, so getting any of them wrong means
deleting the tag and re-tagging.

Tag **the commit on `main`** that the release PR produced, never the branch tip.

```bash
git checkout main && git pull -p
git log -1 --oneline            # confirm this is the release merge
git tag -a vX.Y.Z -m "SofaBuffers corelib-c-cpp X.Y.Z"
git push origin vX.Y.Z
```

Use an **annotated** tag (`-a`). The history is inconsistent here — `v0.9.0` is
annotated, `v0.10.0` is a lightweight tag on a merge commit — and annotated is
the one worth keeping.

Pushing the tag is what fires `Version consistency`. Watch it:

```bash
gh run list --workflow "Version consistency" --limit 3
```

If it fails, a manifest is wrong: fix it on `main` via a `fix(release):` PR, then
move the tag only if the user agrees to re-tag.

## 7. Publish the GitHub release

```bash
gh release create vX.Y.Z --title "vX.Y.Z" --notes "<notes>"
```

Notes follow the shape of `v0.10.0` — check it with `gh release view v0.10.0`:

1. One line placing the library in the family: *"Aligns this library with the rest of the SofaBuffers family at **X.Y.Z**. The git tag is the source of truth for the version; every package manifest matches it."*
2. A **`Breaking since vA.B.C`** section, when there is one, prefaced by the pre-1.0 rule, with one bullet per change naming its **Crucible F-NNNN / CORELIB_PLAN §N** reference and what a consumer must now do differently.
3. A closing line on what else moved in lockstep (e.g. *"sofabgen changed in lockstep and is on `main`."*).

Docs need no action: the `Docs` workflow deploys Pages on every push to `main`,
and Doxygen picks up the new `PROJECT_NUMBER` from CMake automatically.

## Checklist

- [ ] main, clean, CI green
- [ ] version agreed and family-aligned
- [ ] all four manifests bumped — **including `library.properties`**
- [ ] step 4 checks all `ok`, cmake configures
- [ ] `chore(release):` PR based on main, CI green, merged
- [ ] tag named `vX.Y.Z` — lowercase `v`, annotated, on the main merge commit, pushed
- [ ] `Version consistency` workflow green on the tag
- [ ] GitHub release published with family + breaking-change notes
