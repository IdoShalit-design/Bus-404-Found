# auto-fix-issues

Fully unattended issue-fixing agent for this repo, built on the Claude Code CLI. Give it a set of open GitHub issues and it will, without stopping to ask you anything:

1. Create/resume the `auto-fix-issues` branch.
2. Loop through the target issues: read → fix → self-review (`review_agent.md`) → commit `Fix #<ID>` → `gh issue close <ID>`.
3. If it hits Claude's usage limit mid-run, it waits 5 hours and resumes the same session exactly where it left off. It keeps doing this until every issue in scope is handled.

Everything stays on the `auto-fix-issues` branch — nothing is pushed or merged automatically. Review and push it yourself when it's done.

## Files

- `auto_fix_issues.md` — the full task spec fed to Claude Code as its instructions.
- `review_agent.md` — the persona Claude reads and adopts during the review phase of each fix.
- `run_auto_fix.sh` — the driver loop: launches Claude headless, detects usage-limit vs. real errors, sleeps/resumes.

## One-time setup

```bash
# from the repo root
chmod +x scripts/auto-fix-issues/run_auto_fix.sh

# confirm gh is installed and authenticated
gh auth status
# if not:
gh auth login

# confirm claude CLI is logged in
claude auth status   # or just run `claude` once interactively and check
```

Before trusting this to run for hours unattended, do **one manual dry run** on a single low-stakes issue and read the diff it produces. `--dangerously-skip-permissions` means Claude Code will run any shell command it decides to, with no approval step — that's what "no involvement from me" requires, but it also means you should trust the instructions in `auto_fix_issues.md` before letting it loose on everything. Consider running it against a disposable clone/worktree first if you want an extra safety margin.

## Running it

All open issues:

```bash
cd /path/to/Bus-404-Found
nohup scripts/auto-fix-issues/run_auto_fix.sh > /dev/null 2>&1 &
disown
```

Only specific issues:

```bash
ISSUE_SCOPE="12,15,20" nohup scripts/auto-fix-issues/run_auto_fix.sh > /dev/null 2>&1 &
disown
```

Only issues with a label:

```bash
ISSUE_SCOPE="label:bug" nohup scripts/auto-fix-issues/run_auto_fix.sh > /dev/null 2>&1 &
disown
```

`nohup ... &` + `disown` is what lets it survive you closing the terminal, going to a movie, or your machine sleeping/locking (as long as the machine itself stays powered on and awake — a laptop that goes to sleep will pause the process; check your power settings, or run this on a machine/VM that stays on).

## Watching progress

```bash
tail -f .auto_fix_logs/run.log        # driver-level log (sessions, waits, retries)
cat .auto_fix_logs/progress.log       # one line per issue fixed/skipped
git log auto-fix-issues --oneline     # commits so far
```

## Stopping it

```bash
pkill -f run_auto_fix.sh
```

Note this only stops the outer driver loop — if a `claude -p` call is mid-flight it will finish that call before the script notices. Kill twice a few seconds apart if you need it to stop immediately, or just let the current call finish.

## Resuming after a manual stop

Just re-run the same command. `.auto_fix_logs/session_id.txt` holds the last session ID, so it picks the conversation back up automatically. If you want to force a completely fresh start, delete `.auto_fix_logs/` first.

## Known gaps / things to sanity-check on your machine

- `--resume` combined with `-p --output-format json` wasn't tested against a live rate-limit event when this was written — the driver's rate-limit detection is a text-pattern match on the CLI's output (`grep -qiE "rate.?limit|usage limit|..."`), which is inherently a bit brittle if Anthropic changes the wording. Watch the first real rate-limit hit and adjust the pattern in `run_auto_fix.sh` if it doesn't catch it (it falls back to a 5-minute retry loop either way, so it won't hang forever — worst case it retries every 5 min instead of waiting the full 5 hours).
- The workflow closes issues it decides are invalid/duplicate/already-fixed rather than getting stuck — that's a deliberate choice to keep it fully autonomous per your requirement. Check `progress.log` afterward for anything marked `skipped`/`invalid`.
