# GameCube Native Runtime - Project Specification

## Project Overview

Build a native runtime for GameCube games that produces **hardware-equivalent output** without emulation. The runtime translates GX (GameCube graphics API) calls to modern Vulkan/Metal, validated against Dolphin emulator as ground truth.

## Core Philosophy

### Test-Driven AI Development
- AI writes code, but **tests determine correctness**
- No "looks good" - only **bit-exact** validation
- AI iterates until tests pass, with zero interpretation

### Ground Truth
- **Dolphin** = the oracle (accurate GameCube emulator)
- Every operation must produce identical output to Dolphin
- No discussion, no feeling - just bit-exact comparison

### What We Compare Per Subsystem

| Subsystem | Test Method | What We Compare |
|-----------|-------------|-----------------|
| **DVD** | RAM dump | Bytes loaded into memory |
| **OS** | RAM dump | Heap state, thread state |
| **GX** | Pixel compare | Rendered frame (PNG) |
| **DSP/Audio** | Sample compare | PCM audio buffers |
| **SI** | State compare | Controller input registers |
| **VI** | Timing check | Frame count, sync |

---

## Architecture

### High-Level Flow

```
Original Game (DOL/ELF)
         │
         ▼
    Recompiled Code
    (PowerPC → x86/ARM)
         │
         ▼
    GX Runtime Layer
    (GX API → RHI abstraction)
         │
         ▼
    Render Backend
    ┌─────────┬─────────┬─────────┐
    │  Metal  │ Vulkan  │  D3D12  │
    │ (macOS) │ (Linux) │ (Win)   │
    └─────────┴─────────┴─────────┘
         │
         ▼
    Native Executable
```

### Rendering Backend (RT64-style)

Following the approach of RT64 (used by N64Recomp projects):

| Platform | Primary API | Status |
|----------|-------------|--------|
| macOS | **Metal** | Native, best performance |
| Linux | **Vulkan** | Native |
| Windows | **D3D12** / Vulkan | D3D12 preferred |

**Why this approach:**
- Metal is native on macOS (no translation layer overhead)
- Vulkan via MoltenVK adds unnecessary overhead on Mac
- OpenGL is deprecated on macOS (stuck at 4.1)
- Single RHI (Render Hardware Interface) abstracts platform differences

### Key Components

1. **DVD Runtime** - File I/O from disc images
2. **OS Runtime** - Memory management, threads, interrupts
3. **GX Runtime** - Translates GX calls to abstract RHI
4. **RHI Layer** - Platform-agnostic render interface
5. **Render Backends** - Metal/Vulkan/D3D12 implementations
6. **Audio Runtime** - DSP/AI audio processing
7. **Test Harness** - Validates bit-exact output against Dolphin
8. **AI Integration** - LLM assists with code generation within test constraints

---

## DVD Subsystem (Start Here)

DVD is the simplest subsystem to implement and test. Start here before moving to GX.

### Why DVD First?

1. **Simpelste test**: input → RAM bytes → vergelijk
2. **Geen pixels**: geen shaders, geen GPU
3. **Games hebben het nodig**: zonder DVD geen asset loading
4. **Goed te isoleren**: één functie, één test

### How DVD Works

```
Game calls: DVDRead("/Object/Link.arc", dst=0x80300000, size=4096)
            │
            ▼
DVD system reads from disc image
            │
            ▼
Data lands in RAM at destination address
            │
            ▼
Test: Compare RAM[dst..dst+size] with expected bytes
```

### DVD Unit Test Structure

A DVD unit test is a **mini-DOL** that does only one thing:

```c
// test_dvd_read_basic.c
int main() {
    OSInit();
    DVDInit();

    // One DVD operation
    DVDRead("/Object/Link.arc", (void*)0x80300000, 4096);

    // Signal done (or hang)
    while(1) {}
}
```

### DVD Test Flow

```
1. Compile mini-DOL with single DVD operation
2. Run in Dolphin
3. Dump RAM[dst..dst+size] → expected.bin
4. Run same DOL in our runtime
5. Dump same RAM range → actual.bin
6. Compare: expected.bin == actual.bin
7. PASS if identical, FAIL if different
```

### DVD Functions to Test

| Function | What It Does | Test Focus |
|----------|--------------|------------|
| `DVDInit()` | Initialize DVD system | No crash, state correct |
| `DVDOpen()` | Open file handle | Handle valid, no error |
| `DVDRead()` | Sync read to RAM | Bytes in RAM match |
| `DVDReadAsync()` | Async read | Bytes match after callback |
| `DVDClose()` | Close handle | Clean state |
| `DVDGetLength()` | Get file size | Correct size returned |

### Example Test Case

```yaml
test_id: dvd_read_basic_001
file: "/Object/Link.arc"
destination: 0x80300000
size: 4096
expected_hash: sha256:abc123...
validation: RAM[0x80300000..0x80300FFF] == expected bytes
```

---

## OS Subsystem

After DVD, implement OS basics needed by most games.

### OS Functions to Test

| Function | What It Does | Test Focus |
|----------|--------------|------------|
| `OSInit()` | Initialize OS | Heap setup, no crash |
| `OSAlloc()` | Allocate memory | Pointer valid, RAM correct |
| `OSFree()` | Free memory | Heap state clean |
| `OSGetTime()` | Get system time | Timing correct |
| `OSCreateThread()` | Create thread | Thread runs |

### OS Test Method

Same as DVD: mini-DOL → Dolphin → RAM dump → compare.

---

## Testing System

### Unit Test Definition

A unit test is:
- **Input**: Specific GX state + draw parameters
- **Expected Output**: Exact pixels from Dolphin (frame 2)
- **Pass Criteria**: `pixel_diff(expected, actual) == 0`

### Test Structure

```
test_id: gx_blend_alpha_001
input:
  - GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA)
  - GXSetTevOp(GX_TEVSTAGE0, GX_MODULATE)
  - Draw quad with RGBA texture
expected: expected_001.png (from Dolphin)
tolerance: 0 (pixel-exact)
```

### Test Categories

| Category | What It Tests | Validation |
|----------|---------------|------------|
| GX State | Individual GX functions | Pixel-exact |
| TEV | Texture Environment stages | Pixel-exact |
| Blending | Alpha/additive/subtractive | Pixel-exact |
| Z-Buffer | Depth testing modes | Pixel-exact |
| EFB | Embedded framebuffer copies | Pixel-exact |
| Timing | Frame timing behavior | Cycle-accurate |

### Test Automation Loop

```
for each test:
    1. Write GX test description → gx_test.bin
    2. Run in Dolphin (headless) → expected.png
    3. Run in runtime → actual.png
    4. Compare pixels
    5. If FAIL:
       - AI receives: GX state, expected, actual, diff
       - AI modifies runtime code
       - Repeat from step 3
    6. If PASS: next test
```

---

## Pattern Mining Approach

### Concept

Games reuse the same GX patterns with different data. Instead of learning every frame, learn the **building blocks**.

### Pattern Types

1. **GX State Patterns** - Common register combinations
2. **Draw Patterns** - Typical primitive setups (sprite, mesh, UI)
3. **Effect Patterns** - Water, fog, glow, shadows
4. **TEV Patterns** - Texture combine configurations

### Process

```
1. Analyze many games → extract GX call sequences
2. Cluster similar sequences → identify patterns
3. For each pattern:
   - Create minimal test case
   - Run on Dolphin → capture ground truth
   - Build shader template (platform-agnostic)
4. At runtime:
   - Recognize incoming GX pattern
   - Select matching shader template
   - Compile to native backend (Metal/Vulkan/D3D12)
   - Apply with current parameters
```

### Why This Works

- Games use ~2,000 unique GX patterns across 100,000+ draw calls
- Patterns are reusable across different games
- Learning patterns ≠ learning pixels

---

## AI Integration Rules

### What AI Does
- Reads Nintendo SDK documentation for function behavior
- Writes unit tests per SDK function
- Implements runtime code (DVD, OS, GX, etc.)
- Analyzes test failures and proposes fixes
- Logs all sessions for review

### What AI Does NOT Do
- Decide if output "looks correct"
- Skip or interpret test results
- Hallucinate hardware behavior
- Work without test validation
- Argue with Dolphin ground truth

### AI Workflow

```
1. AI reads SDK docs for function X
2. AI writes mini-DOL unit test for X
3. Run in Dolphin → capture expected output
4. AI implements function X in runtime
5. Run same test → capture actual output
6. Compare: expected == actual?
7. If FAIL: AI fixes code, repeat from step 4
8. If PASS: next function
```

### Test-Driven AI Development (Key Insight)

From the original brainstorm:

> "AI zonder tests = vibes"
> "AI met tests = engineering"

The tests are the "straight jacket" that keeps AI productive:
- AI can only move within test boundaries
- No interpretation, only bit-exact comparison
- If test fails, AI must fix - no discussion

### Constraints for AI

1. **Never guess** - If unsure, ask or test
2. **Tests are truth** - Dolphin output is always correct
3. **Small changes** - One fix per iteration
4. **Explain reasoning** - Document why each change was made
5. **Log everything** - Sessions saved for AI #2 to review

---

## File Structure

```
gamecube/
├── docs/
│   ├── PROJECT_SPEC.md          # This file
│   └── chatgpt-chats/           # Original brainstorm conversations
├── runtime/
│   ├── dvd/                     # DVD subsystem (START HERE)
│   │   ├── dvd.h                # DVD API interface
│   │   ├── dvd.cpp              # DVDInit, DVDRead, etc.
│   │   └── disc_image.cpp       # ISO/GCM reading
│   ├── os/                      # OS subsystem
│   │   ├── os.h                 # OS API interface
│   │   ├── os_init.cpp          # OSInit
│   │   ├── os_memory.cpp        # Heap, allocation
│   │   └── os_thread.cpp        # Threading
│   ├── gx/                      # GX API implementation
│   │   ├── blend.cpp            # Blending modes
│   │   ├── tev.cpp              # TEV stages
│   │   ├── texture.cpp          # Texture handling
│   │   └── z_buffer.cpp         # Depth buffer
│   ├── rhi/                     # Render Hardware Interface
│   │   ├── rhi.h                # Abstract interface
│   │   ├── types.h              # Common types
│   │   └── shader_compiler.cpp  # Cross-platform shader handling
│   └── backends/                # Platform-specific implementations
│       ├── metal/               # macOS backend
│       │   ├── metal_device.mm
│       │   └── shaders/         # Metal shaders (.metal)
│       ├── vulkan/              # Linux/Windows backend
│       │   ├── vulkan_device.cpp
│       │   └── shaders/         # SPIR-V shaders
│       └── d3d12/               # Windows backend (optional)
│           ├── d3d12_device.cpp
│           └── shaders/         # HLSL shaders
├── tests/
│   ├── harness/                 # Test runner code
│   ├── sdk/                     # SDK unit tests (mini-DOLs)
│   │   ├── dvd/                 # DVD tests
│   │   │   ├── dvd_read_basic.c
│   │   │   └── dvd_read_async.c
│   │   ├── os/                  # OS tests
│   │   │   └── os_init.c
│   │   └── gx/                  # GX tests
│   │       └── gx_blend_001.c
│   └── expected/                # Dolphin reference data
│       ├── dvd/                 # RAM dumps
│       │   └── dvd_read_basic.bin
│       └── gx/                  # Screenshots
│           └── gx_blend_001.png
├── tools/
│   ├── dolphin_runner.py        # Headless Dolphin automation
│   ├── ram_dump.py              # RAM extraction from Dolphin
│   ├── ram_compare.py           # Binary diff tool
│   ├── pixel_compare.py         # Image diff tool
│   └── pattern_extractor.py     # GX pattern mining
└── .beads/                      # Task tracking
```

---

## Development Phases

### Phase 0: DVD Subsystem (Start Here)
- [ ] Build Dolphin RAM dump tool
- [ ] Create RAM comparison tool
- [ ] Implement DVDInit()
- [ ] Implement DVDRead() (sync)
- [ ] Pass first 5 DVD unit tests

### Phase 1: OS Basics
- [ ] Implement OSInit()
- [ ] Implement basic memory allocation
- [ ] Pass OS unit tests

### Phase 2: Test Infrastructure
- [ ] Build Dolphin headless test runner
- [ ] Create pixel comparison tool
- [ ] Define first 10 GX micro-tests
- [ ] Validate test harness works

### Phase 3: Basic GX Runtime
- [ ] Implement GXSetBlendMode
- [ ] Implement GXSetZMode
- [ ] Implement basic TEV (1 stage)
- [ ] Pass first 10 GX tests

### Phase 4: Pattern Library
- [ ] Extract patterns from 5 games
- [ ] Cluster into pattern categories
- [ ] Create shader templates per pattern
- [ ] Build pattern recognition system

### Phase 5: Full Runtime
- [ ] Complete GX API coverage
- [ ] All TEV stages
- [ ] EFB copies
- [ ] Pass 1000+ tests

### Phase 6: Game Testing
- [ ] Run simple homebrew
- [ ] Run commercial game intro
- [ ] Full game playable

---

## Success Criteria

| Metric | Target |
|--------|--------|
| GX API coverage | 100% of used functions |
| Test pass rate | 100% pixel-exact |
| Pattern coverage | 95% of common patterns |
| Performance | 60 FPS at 1080p minimum |
| Games tested | 10+ commercial titles |

---

## Key Principles

1. **Dolphin is always right** - Never argue with ground truth
2. **Pixel-exact or fail** - No "close enough"
3. **Patterns over pixels** - Learn behavior, not images
4. **AI assists, tests decide** - Human + AI + tests = system
5. **Iterate small** - One test, one fix, one step

---

## For AI Agents

When working on this project:

1. **Read this spec first** - Understand the testing philosophy
2. **Check tests before coding** - Know what "correct" means
3. **Run tests after changes** - Validate immediately
4. **Report failures clearly** - Show expected vs actual
5. **Propose minimal fixes** - One change at a time
6. **Never skip validation** - Tests are mandatory

The goal is not "working code" but "hardware-equivalent code validated by tests."
