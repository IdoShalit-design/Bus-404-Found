# Auto-Fix-Issues Workflow

You are running fully unattended. Do not ask the user for confirmation or clarification at any point in this workflow — you already have full permission to read, write, execute git/gh commands, and modify any file in this repository. If you hit a genuinely ambiguous decision, make the safest reasonable choice yourself, note your reasoning in the commit message, and keep going. Never stop and wait for a human.

This file is idempotent: you may be started fresh, or resumed mid-run after an interruption (context limit, crash, rate limit). Always start by figuring out where things actually stand instead of assuming a clean slate.

## Target issues

By default, target **all open issues** (`gh issue list --state open`).

If the user invoked this run with a specific scope (a list of issue numbers, or a label filter), use that scope instead — check for an environment variable `ISSUE_SCOPE` or arguments passed alongside this prompt. If `ISSUE_SCOPE` is set to a comma-separated list of issue numbers, only process those. If it's set to a label (e.g. `label:bug`), pass that filter to `gh issue list`.

## Step 0 — Resume check

1. Run `git status` and `git branch --show-current`.
2. If you're not already on `auto-fix-issues`, check whether it exists:
   - `git rev-parse --verify auto-fix-issues` — if it exists, `git checkout auto-fix-issues`. Keep whatever commits are already on it; do not reset or rebase away existing work. You are continuing on top of the last commit on this branch.
   - If it doesn't exist, create it from the current default branch: `git checkout -b auto-fix-issues`.
3. If `git status` shows uncommitted changes already in the working tree (this means a previous run was interrupted mid-fix), do not discard them. Figure out which issue they belong to (check recent `git log` messages and the diff content against open issue titles), finish that issue properly (review phase → commit → close) before moving on to the next one.
4. Check for a completion marker at `.auto_fix_logs/ALL_DONE`. If present and no new issues have appeared since, you're done — skip to "Final step" below.

## Step 1 — Fetch issues

Run `gh issue list --state open --json number,title,labels -L 200` (adjusted for `ISSUE_SCOPE` per above). This is your queue. Re-run this at the start of every resume — issues already closed in a prior run will no longer appear, which is what makes this workflow naturally resumable.

## Step 2 — Loop through each issue

For each issue still open, in ascending order by number:

### 2a. Read
Run `gh issue view <ID>` (add `--comments` if the discussion matters). Understand exactly what's being asked. Look at referenced files/errors if any are mentioned.

### 2b. Fix Phase
Implement the fix directly in the working tree:
- Explore the relevant code first (don't guess at file locations — search).
- Make the smallest correct change that resolves the issue.
- If the issue implies a test should exist and the repo has a test setup, add/update one.
- If you determine the issue is not actually reproducible, already fixed, invalid, or is a duplicate: don't force a fake fix. Instead, `gh issue comment <ID>` explaining why, close it with `gh issue close <ID> --comment "..."`, and move to the next issue.

### 2c. Review Phase
Read `review_agent.md` (same directory as this file) and act strictly as the Reviewer Agent it describes. Run `git diff` against the current changes for this issue only, and refactor per its instructions until the diff is clean. This phase edits files directly — no separate approval step.

### 2d. Commit & Close
1. `git add` the relevant files (only the ones related to this fix — don't sweep in unrelated changes).
2. `git commit -m "Fix #<ID>: <short description>"` — the message must be exactly `Fix #<ID>` as the leading token so GitHub links it, followed by a brief human-readable summary.
3. `gh issue close <ID> --comment "Fixed in <commit-sha>. <one-line summary of the fix>."`
4. Move to the next issue in the queue.

## Progress logging

After each issue is committed and closed, append one line to `.auto_fix_logs/progress.log` (create the directory/file if needed): `<timestamp> issue #<ID> — <status: fixed|skipped|invalid> — <commit sha or reason>`.

## Final step

Once every issue in scope has been processed (fixed-and-closed, or explicitly closed-as-invalid with explanation):

1. Create the file `.auto_fix_logs/ALL_DONE` containing a short summary: number of issues fixed, number skipped/invalid, and the commit range.
2. Print a final summary to stdout.
3. Do not open a PR and do not merge to the default branch — leave everything on the `auto-fix-issues` branch for the user to review.
4. Stop.

## If you're about to run out of context or hit a usage limit mid-run

Just stop naturally at a safe point (ideally right after a commit, not mid-edit). The outer script driving this session will detect the interruption, wait, and resume you — Step 0 above is what makes the resume pick up correctly. You don't need to do anything special except avoid leaving the working tree in a half-edited, uncommitted state for an issue if you can help it.
