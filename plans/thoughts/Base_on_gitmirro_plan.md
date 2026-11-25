# Code::Blocks upstream-history surgery plan

## Goal

Convert `codeblocks-macos` from a snapshot-based downstream repository into a maintainable Git fork whose history is rooted in the Code::Blocks Git mirror, while preserving the existing downstream commits and keeping the current repository recoverable at every step.

The operation is intentionally linear. Do not proceed to the next phase until the acceptance check for the current phase passes.

## Known facts

- Current downstream repository: `codeblocks-macos`.
- Current downstream tip at the start of this investigation: `ffe2ff53e`.
- Safety branch already created: `backup/downstream-before-upstream-update-2026-11` at `ffe2ff53e`.
- First downstream Git commit: `2408e77e`.
- `2408e77e` was imported as a source snapshot and has no Git ancestry in the upstream mirror.
- Its commit message records `Import Code::Blocks trunk r14050.`
- The read-only Git mirror is `https://github.com/arnholm/codeblocks_sfmirror.git`.
- In that mirror, commit `d3715f34810398233c9a8473ed2ce023d4a450d7` corresponds to SVN `trunk@14050`.
- Direct tree comparison is noisy because the imported snapshot contains expanded SVN keywords, for example `$Revision: 14024 $`, `$Id: ... $`, and `$HeadURL: ... $`, while the Git mirror contains unexpanded `$Revision$`, `$Id$`, and `$HeadURL$`.
- Therefore byte-for-byte tree identity is not a prerequisite for the migration.

## Principle

Do **not** flatten the downstream work into the roughly 5 MB `2408e77e..main` mega-patch.

Instead, preserve the existing downstream commit sequence and replay it onto the real upstream Git commit corresponding to SVN r14050. This creates genuine common ancestry for all future upstream updates.

Old history:

    2408e77e  snapshot of trunk r14050
        |
        +-- downstream commit 1
        +-- downstream commit 2
        +-- ...
        +-- ffe2ff53e / current downstream

Target history:

    upstream Git history
        |
    d3715f348  (SVN trunk@14050)
        |
        +-- replayed downstream commit 1
        +-- replayed downstream commit 2
        +-- ...
        +-- migrated downstream tip
        |
        +-- future upstream merges

The original repository/history remains available as the recovery reference.

---

# Phase 0 - Freeze and record the starting state

Run in `codeblocks-macos`:

```bash
git status
git rev-parse HEAD
git rev-parse backup/downstream-before-upstream-update-2026-11
git log --oneline --decorate --graph -20
```

Expected starting tip:

```text
ffe2ff53e
```

If there are uncommitted changes, commit them before continuing. Do not use `git stash` as the long-term safety mechanism for this migration.

Create one additional immutable-looking tag for the migration boundary:

```bash
git tag -a pre-upstream-history-migration-2026-11 \
    ffe2ff53e \
    -m "Downstream state before attaching Code::Blocks Git upstream history"
```

### Acceptance check

```bash
git status --short
git show-ref --tags pre-upstream-history-migration-2026-11
git show-ref --heads backup/downstream-before-upstream-update-2026-11
```

`git status --short` must be empty and both safety references must resolve.

---

# Phase 1 - Record the upstream anchor

In `codeblocks-upstream-git`:

```bash
git show -s --format=fuller d3715f34810398233c9a8473ed2ce023d4a450d7
```

Confirm that the commit message contains:

```text
git-svn-id: https://svn.code.sf.net/p/codeblocks/code/trunk@14050 ...
```

Optionally create a local descriptive tag in the mirror:

```bash
git tag upstream-svn-r14050 d3715f34810398233c9a8473ed2ce023d4a450d7
```

### Acceptance check

```bash
git rev-parse upstream-svn-r14050
```

Expected hash:

```text
d3715f34810398233c9a8473ed2ce023d4a450d7
```

---

# Phase 2 - Quantify snapshot-only differences before surgery

This phase does not require perfect equality. Its purpose is to identify systematic transformations so that they are not mistaken for Thomas's downstream changes during conflict resolution.

Use the already identified representative comparison:

```bash
diff -u \
  /tmp/cb-upstream/src/plugins/autosave/autosave.cpp \
  /tmp/cb-old/src/plugins/autosave/autosave.cpp | head -80
```

Known expected difference:

```text
$Revision$   -> $Revision: ... $
$Id$         -> $Id: ... $
$HeadURL$    -> $HeadURL: ... $
```

Produce a count of differing files for reference, but do not try to fix them:

```bash
diff -qr /tmp/cb-old /tmp/cb-upstream > /tmp/r14050-tree-differences.txt || true
wc -l /tmp/r14050-tree-differences.txt
```

### Acceptance check

Manually inspect several unrelated `.cpp` files from the difference list. If their differences are only SVN keyword expansion, record that fact and continue. If genuine source-code differences appear, record the filenames before continuing. They will need explicit attention during replay.

Do **not** rewrite `2408e77e` to normalize these files.

---

# Phase 3 - Bring the upstream mirror into the downstream repository

The objective is to have both histories available in one Git object database while changing no downstream branch yet.

In `codeblocks-macos`:

```bash
git remote add upstream https://github.com/arnholm/codeblocks_sfmirror.git
git fetch upstream
```

Verify that the upstream r14050 object exists locally:

```bash
git cat-file -t d3715f34810398233c9a8473ed2ce023d4a450d7
git show -s --oneline d3715f34810398233c9a8473ed2ce023d4a450d7
```

Expected first command:

```text
commit
```

### Acceptance check

```bash
git status --short
git rev-parse main
git rev-parse pre-upstream-history-migration-2026-11
```

`main` must still point to the original downstream history. Nothing has been migrated yet.

---

# Phase 4 - Enumerate the downstream commit series

Identify every commit after the snapshot import:

```bash
git log --reverse --oneline 2408e77e..pre-upstream-history-migration-2026-11
```

Save the exact ordered list:

```bash
git rev-list --reverse \
    2408e77e..pre-upstream-history-migration-2026-11 \
    > /tmp/codeblocks-downstream-commits.txt

wc -l /tmp/codeblocks-downstream-commits.txt
cat /tmp/codeblocks-downstream-commits.txt
```

Before replay, inspect whether the downstream history contains merge commits:

```bash
git rev-list --merges \
    2408e77e..pre-upstream-history-migration-2026-11
```

### Decision gate

- **No merge commits:** continue with the linear cherry-pick procedure below.
- **Merge commits present:** stop before Phase 5. Preserve-merges requires a different replay strategy and should be planned from the actual merge topology rather than blindly cherry-picking first-parent commits.

---

# Phase 5 - Create a migration branch at the real upstream r14050 commit

Do not modify `main`.

```bash
git switch --detach d3715f34810398233c9a8473ed2ce023d4a450d7
git switch -c migration/r14050-downstream-replay
```

Sanity check:

```bash
git log -1 --format='%H%n%B'
```

The base must be `d3715f348...` and its message must identify SVN `trunk@14050`.

---

# Phase 6 - Replay downstream commits in original order

Replay commits **one at a time**, not as one opaque range. This is slower but provides a precise checkpoint and isolates conflicts to the feature that created them.

For each hash in `/tmp/codeblocks-downstream-commits.txt`:

```bash
git cherry-pick <hash>
```

If the commit applies cleanly:

```bash
git status
```

Then continue to the next commit.

If a conflict occurs:

```bash
git status
git diff --name-only --diff-filter=U
git diff --cc
```

Resolve using this decision order:

1. Determine the intention of the original downstream commit:

   ```bash
   git show <hash>
   ```

2. Compare the conflicting upstream file around r14050/current structure.
3. Ignore differences caused solely by SVN keyword expansion.
4. Preserve the behavior introduced by the downstream commit, not necessarily its old textual implementation.
5. If the upstream mirror already contains an equivalent fix, prefer upstream and consider the downstream commit obsolete.
6. If both upstream and downstream changes are required, integrate them deliberately.
7. Build or run a focused test when the conflict affects a critical subsystem.

After resolution:

```bash
git add <resolved-files>
git cherry-pick --continue
```

If investigation demonstrates that the commit is genuinely obsolete because upstream already contains it:

```bash
git cherry-pick --skip
```

Record skipped commits and the reason in a migration note. Do not skip merely to make the replay advance.

If a resolution becomes questionable:

```bash
git cherry-pick --abort
```

The migration branch can then be reset/recreated without affecting `main` or the safety references.

---

# Phase 7 - Verify the replay against the old downstream tip

At this point the migrated tree should represent the old downstream behavior on the genuine upstream history.

Because SVN keywords differ, a raw tree hash comparison is expected to fail. Compare semantically instead.

First inspect the aggregate difference:

```bash
git diff --stat pre-upstream-history-migration-2026-11..migration/r14050-downstream-replay
```

Then inspect the actual diff:

```bash
git diff \
    pre-upstream-history-migration-2026-11..migration/r14050-downstream-replay
```

Classify every difference as one of:

- SVN keyword normalization.
- Release/snapshot generated artifact difference.
- Intentionally skipped downstream change because upstream superseded it.
- Intentional conflict resolution adaptation.
- **Unexpected functional difference.** This category blocks migration acceptance.

Useful targeted check for keyword noise:

```bash
git diff \
    pre-upstream-history-migration-2026-11..migration/r14050-downstream-replay \
    | grep -E '\$Revision|\$Id|\$HeadURL'
```

### Acceptance check

No unexplained functional differences remain.

---

# Phase 8 - Build and smoke-test before updating upstream

First prove that the history surgery itself did not break the known r14050-based downstream product.

At minimum on macOS:

- Code::Blocks builds successfully.
- App bundle launches.
- Existing Apple Silicon/macOS-specific functionality still works.
- Native debugger integration works.
- Existing downstream project/wizard/distribution features still work.

If Windows is readily available, perform the established Windows smoke test too. Ubuntu validation can remain explicitly pending because Ubuntu access is intermittent.

### Acceptance check

The migrated r14050-based branch behaves like the pre-migration downstream product on currently available platforms.

Do **not** combine history surgery and current-upstream integration into one debugging problem.

---

# Phase 9 - Advance from upstream r14050 to current upstream

Only after Phase 8 passes should the branch consume post-r14050 upstream changes.

Determine the mirror's current upstream branch first:

```bash
git remote show upstream
```

Use the reported HEAD branch below as `<upstream-branch>`.

Fetch current upstream:

```bash
git fetch upstream
```

Create a second integration branch so the successful history migration remains preserved:

```bash
git switch migration/r14050-downstream-replay
git switch -c integration/current-upstream
```

Merge current upstream:

```bash
git merge --no-ff upstream/<upstream-branch>
```

Resolve conflicts feature-by-feature. At this stage Git now has genuine common ancestry at r14050, so these are real upstream-vs-downstream conflicts rather than snapshot noise.

For each conflict:

```bash
git status
git diff --name-only --diff-filter=U
git diff --cc
```

Classify conflict resolution as:

1. upstream supersedes downstream fix;
2. downstream behavior still required;
3. both required;
4. generated/build metadata should be regenerated rather than hand-merged.

### Acceptance check

```bash
git status
git log --graph --decorate --oneline -30
```

Working tree clean, merge complete, history visibly contains both the upstream line and downstream line.

---

# Phase 10 - Full validation of current upstream integration

Validate in increasing cost order.

## macOS

1. clean/configured build;
2. launch application bundle;
3. create/open/rebuild project;
4. native Apple Silicon GDB/debugger smoke test;
5. debugger watches/expressions previously fixed downstream;
6. plugin loading;
7. project wizard paths relevant to the downstream distribution;
8. packaging/bundle creation if applicable.

## Windows

When available, validate the established Code::Blocks distribution, installer, debugger, and wizard workflows.

## Ubuntu

When the Ubuntu machine is next available:

1. build using the repository's Debian/Ubuntu README procedure;
2. build `.deb` packages;
3. install the resulting package set with APT;
4. reproduce the CodeSnippets wxGTK initialization problem before/after its local fix if still applicable;
5. launch with a clean profile;
6. compile/run/debug a simple C++ project;
7. exercise contrib plugins;
8. package the Linux release archive only after this validation passes.

Do not let lack of immediate Ubuntu access block the Git-history migration or macOS/Windows upstream integration. Record Ubuntu status as pending instead.

---

# Phase 11 - Promote the migrated history

Only after current-upstream integration and available-platform validation pass should the migrated branch replace `main`.

First preserve the old main permanently:

```bash
git branch legacy/snapshot-based-main pre-upstream-history-migration-2026-11
```

Then move `main` deliberately. One safe option:

```bash
git switch main
git reset --hard integration/current-upstream
```

Check:

```bash
git status
git log --graph --decorate --oneline -30
```

Do not force-push until the local result has been reviewed.

Because this rewrites the ancestry of the downstream commits, updating an existing remote `main` will require a history rewrite. If/when publishing:

```bash
git push origin legacy/snapshot-based-main
git push origin pre-upstream-history-migration-2026-11
```

Then, only when intentionally replacing remote `main`:

```bash
git push --force-with-lease origin main
```

Use `--force-with-lease`, never an unqualified `--force`.

---

# Phase 12 - Establish the permanent maintenance model

After migration, keep:

```text
origin    = Thomas's downstream Code::Blocks repository
upstream  = read-only Code::Blocks Git mirror
```

Routine upstream synchronization becomes:

```bash
git switch main
git status
git fetch upstream
git switch -c integration/upstream-YYYY-MM-DD
git merge --no-ff upstream/<upstream-branch>
```

Resolve and validate there. Only merge/fast-forward back to `main` after tests pass.

Before each upstream merge, create a lightweight recovery tag if useful:

```bash
git tag before-upstream-YYYY-MM-DD main
```

This preserves the working style already used successfully elsewhere: one bounded integration operation, explicit baseline, reproducible tests, and no speculative cleanup mixed into the update.

---

# Stop conditions

Stop and investigate rather than pushing through if any of these occur:

- `d3715f348...` does not identify SVN `trunk@14050`.
- downstream history contains merge topology not accounted for by the replay plan;
- a replay conflict cannot be explained in terms of a specific downstream commit versus upstream r14050;
- the migrated r14050 tree has unexplained functional differences from the old downstream tip;
- macOS build/debugger behavior regresses before current upstream is merged;
- the current upstream merge produces broad unexplained deletions or wholesale generated-file replacement.

At every stop condition, the original `main`, `backup/downstream-before-upstream-update-2026-11`, and `pre-upstream-history-migration-2026-11` references remain available.

---

# Definition of done

The surgery is complete when:

1. downstream history descends from the Git mirror's `d3715f348...` / SVN r14050 commit;
2. meaningful downstream commit history has been preserved rather than flattened;
3. current upstream has been merged using genuine common ancestry;
4. all conflict resolutions are explainable;
5. macOS validation passes;
6. Windows validation passes when available/required for the release;
7. Ubuntu validation is either passed or explicitly marked pending;
8. the old snapshot-based history remains recoverable;
9. `upstream` is configured as the permanent read-only Code::Blocks mirror;
10. future upstream updates can be handled with ordinary `git fetch` + integration-branch `git merge`.
