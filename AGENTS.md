# Agent Instructions

This project uses **bd** (beads) for issue tracking. Run `bd onboard` to get started.

## Task Management Rules

**ALWAYS create tasks before starting work:**
- Break work into small, testable tasks
- Use `bd create "Title" --body "Description"`
- Set dependencies with `bd dep add <blocked> <blocker>`

**ONLY close tasks when CONFIRMED complete:**
- Task must be tested and verified working
- Never close based on "should work" - must be proven
- If unsure, leave open and add comment
- Use `bd close <id> --reason "why"` with clear reason

**Task workflow:**
```bash
bd create "Task title" --body "Description"  # Create
bd update <id> --status in_progress          # Start
# ... do the work ...
# ... TEST and VERIFY ...
bd close <id> --reason "Tested: works"       # Only after confirmed
```

## Quick Reference

```bash
bd ready              # Find available work
bd show <id>          # View issue details
bd update <id> --status in_progress  # Claim work
bd close <id>         # Complete work
bd sync               # Sync with git
```

## Landing the Plane (Session Completion)

**When ending a work session**, you MUST complete ALL steps below. Work is NOT complete until `git push` succeeds.

**MANDATORY WORKFLOW:**

1. **File issues for remaining work** - Create issues for anything that needs follow-up
2. **Run quality gates** (if code changed) - Tests, linters, builds
3. **Update issue status** - Close finished work, update in-progress items
4. **PUSH TO REMOTE** - This is MANDATORY:
   ```bash
   git pull --rebase
   bd sync
   git push
   git status  # MUST show "up to date with origin"
   ```
5. **Clean up** - Clear stashes, prune remote branches
6. **Verify** - All changes committed AND pushed
7. **Hand off** - Provide context for next session

**CRITICAL RULES:**
- Work is NOT complete until `git push` succeeds
- NEVER stop before pushing - that leaves work stranded locally
- NEVER say "ready to push when you are" - YOU must push
- If push fails, resolve and retry until it succeeds

