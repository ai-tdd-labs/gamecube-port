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

## Test-First Workflow (VERPLICHT)

Bij nieuwe SDK functie implementeren, volg ALTIJD deze volgorde:

```
1. LEES SDK eerst
   → src/dolphin/ in MP4 decomp bekijken
   → Begrijp wat de functie doet, parameters, returns

2. ONTWERP test
   → Wat checken we? Welke RAM adressen?
   → Schrijf test case beschrijving

3. BOUW test DOL
   → Mini-DOL die alleen die ene functie test
   → Gebruik devkitPPC (zie /build-dol skill)

4. RUN in Dolphin → expected.bin
   → Draai DOL in Dolphin met GDB stub
   → Dump RAM naar expected.bin (zie /dolphin-debug skill)

5. PAS DAN: implementeer runtime
   → Nu mag je code schrijven
   → Test tegen expected.bin (zie /ram-compare skill)
```

**REGELS:**
- ❌ NOOIT: task "Implement DVDInit()" zonder test
- ✅ ALTIJD: task "Test voor DVDInit ontwerpen" eerst
- ❌ NOOIT: code schrijven voordat expected.bin bestaat
- ✅ ALTIJD: Dolphin output = waarheid

**SDK locatie:**
```
/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/
├── src/dolphin/     # SDK source code
│   ├── dvd/         # DVD functies
│   ├── os/          # OS functies
│   └── gx/          # Graphics functies
└── include/dolphin/ # Headers
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

