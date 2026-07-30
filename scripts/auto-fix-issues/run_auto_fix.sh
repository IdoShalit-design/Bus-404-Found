#!/usr/bin/env bash
# Autonomous issue-fixing driver for the auto-fix-issues workflow.
#
# Launches Claude Code headless (`-p`) with all permission prompts bypassed,
# and keeps the workflow alive across context limits / usage-limit errors by
# waiting 5 hours and resuming the same Claude Code session where it left off.
#
# Usage:
#   ./run_auto_fix.sh                            # all open issues
#   ISSUE_SCOPE="12,15,20" ./run_auto_fix.sh      # only these issue numbers
#   ISSUE_SCOPE="label:bug" ./run_auto_fix.sh     # only issues with this label
#
# Run it in the background so it survives closing the terminal / sleep:
#   nohup ./run_auto_fix.sh > /dev/null 2>&1 &
#   disown
#
# Stop it:
#   pkill -f run_auto_fix.sh   (or find the PID and kill it)
#
# NOTE: verify --resume works together with -p/--print and --output-format json
# on your installed Claude Code version (`claude --version`) before relying on
# this unattended — flags can change between releases. Test one manual
# interrupt+resume cycle first.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"   # scripts/auto-fix-issues -> repo root
PROMPT_FILE="$SCRIPT_DIR/auto_fix_issues.md"
LOG_DIR="$REPO_DIR/.auto_fix_logs"
SESSION_FILE="$LOG_DIR/session_id.txt"
DONE_MARKER="$LOG_DIR/ALL_DONE"
RUN_LOG="$LOG_DIR/run.log"
LAST_OUTPUT="$LOG_DIR/last_output.json"
RATE_LIMIT_WAIT=$((5 * 60 * 60))   # 5 hours, per spec
ERROR_RETRY_WAIT=$((5 * 60))       # 5 minutes for transient/non-rate-limit failures

mkdir -p "$LOG_DIR"
cd "$REPO_DIR"

log() { echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" | tee -a "$RUN_LOG"; }

UUID_RE='^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'

# Print a lowercase v4 UUID. Tries each source and validates the result, so a
# non-working candidate (e.g. the Microsoft Store `python3` stub on Windows,
# which prints a promo message instead of a UUID) is skipped rather than
# silently producing an empty/garbage session ID.
gen_uuid() {
  local out cmd
  if [[ -r /proc/sys/kernel/random/uuid ]]; then
    out="$(tr -d '\r\n' < /proc/sys/kernel/random/uuid)"
    [[ $out =~ $UUID_RE ]] && { printf '%s' "$out"; return 0; }
  fi
  for cmd in uuidgen python3 python py; do
    command -v "$cmd" >/dev/null 2>&1 || continue
    if [[ $cmd == uuidgen ]]; then
      out="$(uuidgen 2>/dev/null | tr -d '\r\n' | tr 'A-Z' 'a-z')"
    else
      out="$("$cmd" -c 'import uuid;print(uuid.uuid4())' 2>/dev/null | tr -d '\r\n')"
    fi
    [[ $out =~ $UUID_RE ]] && { printf '%s' "$out"; return 0; }
  done
  # Last resort: bash's own RANDOM, shaped like a v4 UUID.
  printf '%04x%04x-%04x-4%03x-%04x-%04x%04x%04x' \
    $((RANDOM % 65536)) $((RANDOM % 65536)) $((RANDOM % 65536)) \
    $((RANDOM % 4096)) $(((RANDOM % 16384) + 32768)) \
    $((RANDOM % 65536)) $((RANDOM % 65536)) $((RANDOM % 65536))
}

if [[ ! -f "$PROMPT_FILE" ]]; then
  log "ERROR: prompt file not found at $PROMPT_FILE"
  exit 1
fi

if ! command -v claude >/dev/null 2>&1; then
  log "ERROR: claude CLI not found on PATH."
  exit 1
fi

if ! command -v gh >/dev/null 2>&1; then
  log "ERROR: gh CLI not found on PATH. Install it and run 'gh auth login' first."
  exit 1
fi

if ! gh auth status >/dev/null 2>&1; then
  log "ERROR: gh CLI is not authenticated. Run 'gh auth login' first."
  exit 1
fi

log "=== auto-fix-issues run starting (repo: $REPO_DIR) ==="
if [[ -n "${ISSUE_SCOPE:-}" ]]; then
  log "Scope: ISSUE_SCOPE=$ISSUE_SCOPE"
else
  log "Scope: all open issues"
fi

while true; do
  rm -f "$DONE_MARKER"

  if [[ -f "$SESSION_FILE" ]]; then
    SESSION_ID="$(cat "$SESSION_FILE")"
    log "Resuming session $SESSION_ID"
    claude -p "Resume the auto-fix-issues workflow exactly where you left off. Re-check 'git status' and re-run 'gh issue list --state open' before continuing, per Step 0 of the workflow (auto_fix_issues.md)." \
      --resume "$SESSION_ID" \
      --dangerously-skip-permissions \
      --output-format json \
      > "$LAST_OUTPUT" 2>>"$RUN_LOG"
    EXIT_CODE=$?
  else
    SESSION_ID="$(gen_uuid)"
    if [[ ! $SESSION_ID =~ $UUID_RE ]]; then
      log "ERROR: could not generate a session UUID (got '$SESSION_ID')."
      exit 1
    fi
    log "Starting new session $SESSION_ID"
    claude -p "$(cat "$PROMPT_FILE")" \
      --session-id "$SESSION_ID" \
      --dangerously-skip-permissions \
      --output-format json \
      > "$LAST_OUTPUT" 2>>"$RUN_LOG"
    EXIT_CODE=$?
    echo "$SESSION_ID" > "$SESSION_FILE"
  fi

  if [[ -f "$DONE_MARKER" ]]; then
    log "ALL_DONE marker found. Workflow complete."
    cat "$DONE_MARKER" | tee -a "$RUN_LOG"
    break
  fi

  # Detect usage/rate-limit errors in the captured output (text may vary by version).
  if grep -qiE "rate.?limit|usage limit|quota exceeded|try again (in|after)|resets? at" "$LAST_OUTPUT" "$RUN_LOG" 2>/dev/null; then
    log "Usage limit detected. Sleeping 5 hours before resuming..."
    sleep "$RATE_LIMIT_WAIT"
    continue
  fi

  if [[ $EXIT_CODE -ne 0 ]]; then
    log "claude exited with code $EXIT_CODE (non-rate-limit error). Waiting 5 minutes and retrying..."
    sleep "$ERROR_RETRY_WAIT"
    continue
  fi

  # Clean exit but no ALL_DONE marker yet (e.g. hit a turn/context limit mid-run).
  log "Session ended without ALL_DONE marker. Resuming shortly."
  sleep 30
done

log "=== auto-fix-issues run finished ==="
