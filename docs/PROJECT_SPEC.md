# GameCube Native Runtime - Project Specification

## Project Overview

Build a native runtime for GameCube games that produces **hardware-equivalent output** without emulation. The runtime translates GX (GameCube graphics API) calls to modern Vulkan/Metal, validated against Dolphin emulator as ground truth.

## Core Philosophy

### Test-Driven AI Development
- AI writes code, but **tests determine correctness**
- No "looks good" - only **pixel-exact, RAM-exact, audio-exact** validation
- AI iterates until tests pass, with zero interpretation

### Ground Truth
- **Dolphin** = the oracle (accurate GameCube emulator)
- Every GX operation must produce identical output to Dolphin
- Frame 2 of each test = stable reference image

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

1. **GX Runtime** - Translates GX calls to abstract RHI
2. **RHI Layer** - Platform-agnostic render interface
3. **Render Backends** - Metal/Vulkan/D3D12 implementations
4. **Test Harness** - Validates pixel-exact output against Dolphin
5. **Pattern Library** - Database of known GX behavior patterns
6. **AI Integration** - LLM assists with code generation within test constraints

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
- Writes GX→RHI translation code
- Generates platform-agnostic shader templates
- Implements backend-specific code (Metal/Vulkan/D3D12)
- Analyzes test failures and proposes fixes
- Identifies patterns in game code

### What AI Does NOT Do
- Decide if output "looks correct"
- Skip or interpret test results
- Hallucinate hardware behavior
- Work without test validation

### AI Workflow

```
Human defines: test requirements, GX specifications
AI proposes: implementation code
Tests verify: pixel-exact correctness
AI iterates: until tests pass
```

### Constraints for AI

1. **Never guess** - If unsure, ask or test
2. **Tests are truth** - Dolphin output is always correct
3. **Small changes** - One fix per iteration
4. **Explain reasoning** - Document why each change was made

---

## File Structure

```
gamecube/
├── docs/
│   ├── PROJECT_SPEC.md          # This file
│   └── chatgpt-chats/           # Original brainstorm conversations
├── runtime/
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
│   ├── cases/                   # Individual test definitions
│   │   ├── gx_blend_001.bin
│   │   └── gx_tev_001.bin
│   └── expected/                # Dolphin reference images
│       ├── gx_blend_001.png
│       └── gx_tev_001.png
├── tools/
│   ├── dolphin_runner.py        # Headless Dolphin automation
│   ├── pixel_compare.py         # Image diff tool
│   └── pattern_extractor.py     # GX pattern mining
└── .beads/                      # Task tracking
```

---

## Development Phases

### Phase 1: Test Infrastructure
- [ ] Build Dolphin headless test runner
- [ ] Create pixel comparison tool
- [ ] Define first 10 GX micro-tests
- [ ] Validate test harness works

### Phase 2: Basic GX Runtime
- [ ] Implement GXSetBlendMode
- [ ] Implement GXSetZMode
- [ ] Implement basic TEV (1 stage)
- [ ] Pass first 10 tests

### Phase 3: Pattern Library
- [ ] Extract patterns from 5 games
- [ ] Cluster into pattern categories
- [ ] Create shader templates per pattern
- [ ] Build pattern recognition system

### Phase 4: Full Runtime
- [ ] Complete GX API coverage
- [ ] All TEV stages
- [ ] EFB copies
- [ ] Pass 1000+ tests

### Phase 5: Game Testing
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
