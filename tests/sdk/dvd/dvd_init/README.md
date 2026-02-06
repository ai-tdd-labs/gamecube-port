# Legacy-migrated DOL testcase(s)

This suite was migrated from `docs/_legacy/tests/dols/...`.

Important:
- Many legacy testcases are SYNTHETIC (they define/stub SDK behavior in the test C file, or use libogc APIs instead of the decomp SDK).
- Treat these as scaffolding and a place-holder for future canonical, evidence-based tests.

Canonical rule:
- A canonical test must derive behavior from decompiled SDK sources and/or Dolphin runtime evidence, not from stubs.
