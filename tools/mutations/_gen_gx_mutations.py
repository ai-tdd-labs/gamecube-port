#!/usr/bin/env python3
"""Generate mutation test patches for all GX functions.

Produces proper unified diff patches that apply cleanly with `git apply`.
"""

import os
import sys
import textwrap

MUTATIONS_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.normpath(os.path.join(MUTATIONS_DIR, "..", ".."))
GX_C_PATH = os.path.join(REPO_ROOT, "src", "sdk_port", "gx", "GX.c")

# Read source file once
with open(GX_C_PATH, "r") as f:
    SOURCE_LINES = f.readlines()


def find_line(old_text, after_line=0, occurrence=1):
    """Find line number (1-based) of old_text in source, optionally after a given line."""
    count = 0
    for i, line in enumerate(SOURCE_LINES):
        if i < after_line:
            continue
        if line.rstrip("\n") == old_text:
            count += 1
            if count == occurrence:
                return i + 1  # 1-based
    return None


def gen_patch(old_text, new_text, after_line=0, occurrence=1):
    """Generate a unified diff patch for a single-line change.

    Uses 3 lines of context before and after to ensure unique matching.
    """
    lineno = find_line(old_text, after_line=after_line, occurrence=occurrence)
    if lineno is None:
        return None, old_text

    # 0-based index
    idx = lineno - 1

    # Gather up to 3 lines of context before and after
    ctx_before_start = max(0, idx - 3)
    ctx_after_end = min(len(SOURCE_LINES), idx + 4)

    before_ctx = SOURCE_LINES[ctx_before_start:idx]
    after_ctx = SOURCE_LINES[idx + 1:ctx_after_end]

    # Build hunk
    old_start = ctx_before_start + 1  # 1-based
    old_count = len(before_ctx) + 1 + len(after_ctx)
    new_count = old_count  # same count (replace 1 line with 1 line)

    lines = []
    lines.append("diff --git a/src/sdk_port/gx/GX.c b/src/sdk_port/gx/GX.c")
    lines.append("--- a/src/sdk_port/gx/GX.c")
    lines.append("+++ b/src/sdk_port/gx/GX.c")
    lines.append(f"@@ -{old_start},{old_count} +{old_start},{new_count} @@")

    for cl in before_ctx:
        lines.append(" " + cl.rstrip("\n"))

    lines.append("-" + old_text)
    lines.append("+" + new_text)

    for cl in after_ctx:
        lines.append(" " + cl.rstrip("\n"))

    return "\n".join(lines) + "\n", None


def gen_sh(patch_name, test_dir, desc):
    return textwrap.dedent(f"""\
        #!/usr/bin/env bash
        set -euo pipefail

        # Mutation check for {desc}
        #
        # Usage:
        #   tools/mutations/{patch_name}.sh <test_case_dir> [<test_case_dir2> ...]

        repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
        cd "$repo_root"

        if [[ $# -lt 1 ]]; then
          echo "usage: $0 <test_case_dir> [<test_case_dir2> ...]" >&2
          exit 2
        fi

        args=("tools/run_mutation_check.sh" "tools/mutations/{patch_name}.patch" "--")

        first=1
        for d in "$@"; do
          if [[ $first -eq 0 ]]; then
            args+=("::")
          fi
          first=0
          args+=("tools/run_host_scenario.sh" "$d")
        done

        "${{args[@]}}"
    """)


# Mutation definitions: (patch_name, test_dir, desc, old_line, new_line, after_line, occurrence)
# after_line: 0-based minimum line to start searching (to disambiguate duplicates)
# occurrence: which occurrence of old_line to target (1-based)
MUTATIONS = [
    # 1
    ("gx_adjust_for_overscan_hor_times3",
     "gx_adjust_for_overscan",
     "GXAdjustForOverscan: change hor*2 to hor*3",
     "    u16 hor2 = (u16)(hor * 2u);",
     "    u16 hor2 = (u16)(hor * 3u); // MUTANT",
     0, 1),
    # 2
    ("gx_begin_or_to_and",
     "gx_begin",
     "GXBegin: change OR to AND in vtxfmt|type",
     "    gc_gx_fifo_begin_u8 = (u32)(vtxfmt | type);",
     "    gc_gx_fifo_begin_u8 = (u32)(vtxfmt & type); // MUTANT",
     0, 1),
    # 3
    ("gx_begin_display_list_zero",
     "gx_begin_display_list",
     "GXBeginDisplayList: set in_disp_list to 0 instead of 1",
     "    gc_gx_in_disp_list = 1;",
     "    gc_gx_in_disp_list = 0; // MUTANT",
     0, 1),
    # 4
    ("gx_call_display_list_zero_nbytes",
     "gx_call_display_list",
     "GXCallDisplayList: zero out nbytes",
     "    gc_gx_call_dl_nbytes = nbytes;",
     "    gc_gx_call_dl_nbytes = 0; // MUTANT",
     0, 1),
    # 5
    ("gx_clear_bounding_box_decrement",
     "gx_clear_bounding_box",
     "GXClearBoundingBox: decrement instead of increment",
     "    gc_gx_clear_bounding_box_calls++;",
     "    gc_gx_clear_bounding_box_calls--; // MUTANT",
     0, 1),
    # 6
    ("gx_clear_gp_metric_set_one",
     "gx_clear_gp_metric",
     "GXClearGPMetric: set perf0 to 1 instead of 0",
     "    gc_gx_gp_perf0 = 0;",
     "    gc_gx_gp_perf0 = 1; // MUTANT",
     2066, 1),  # in GXClearGPMetric (line ~2068)
    # 7
    ("gx_clear_mem_metric_loop_bound",
     "gx_clear_mem_metric",
     "GXClearMemMetric: loop bound 9 instead of 10",
     "    for (u32 i = 0; i < 10; i++) gc_gx_mem_metrics[i] = 0;",
     "    for (u32 i = 0; i < 9; i++) gc_gx_mem_metrics[i] = 0; // MUTANT",
     0, 1),
    # 8
    ("gx_clear_pix_metric_loop_bound",
     "gx_clear_pix_metric",
     "GXClearPixMetric: loop bound 5 instead of 6",
     "    for (u32 i = 0; i < 6; i++) gc_gx_pix_metrics[i] = 0;",
     "    for (u32 i = 0; i < 5; i++) gc_gx_pix_metrics[i] = 0; // MUTANT",
     0, 1),
    # 9
    ("gx_clear_vcache_metric_set_one",
     "gx_clear_vcache_metric",
     "GXClearVCacheMetric: set to 1 instead of 0",
     "    gc_gx_vcache_sel = 0;",
     "    gc_gx_vcache_sel = 1; // MUTANT",
     2080, 1),  # in GXClearVCacheMetric (~2082)
    # 10
    ("gx_clear_vtx_desc_wrong_dirty",
     "gx_clear_vtx_desc",
     "GXClearVtxDesc: change dirty state bit from 8 to 4",
     "    gc_gx_dirty_state |= 8u;",
     "    gc_gx_dirty_state |= 4u; // MUTANT",
     806, 1),  # in GXClearVtxDesc (~814)
    # 11
    ("gx_color_1x8_zero",
     "gx_color_1x8",
     "GXColor1x8: always store 0",
     "    gc_gx_color1x8_last = (u32)c;",
     "    gc_gx_color1x8_last = 0; // MUTANT",
     0, 1),
    # 12
    ("gx_color_3u8_swap_rb",
     "gx_color_3u8",
     "GXColor3u8: swap r and b channels",
     "    gc_gx_color3u8_last = ((u32)r << 16) | ((u32)g << 8) | (u32)b;",
     "    gc_gx_color3u8_last = ((u32)b << 16) | ((u32)g << 8) | (u32)r; // MUTANT",
     0, 1),
    # 13
    ("gx_copy_disp_zero_clear",
     "gx_copy_disp",
     "GXCopyDisp: zero out clear flag",
     "    gc_gx_copy_disp_clear = (u32)clear;",
     "    gc_gx_copy_disp_clear = 0; // MUTANT",
     0, 1),
    # 14
    ("gx_copy_tex_wrong_bp_reg",
     "gx_copy_tex",
     "GXCopyTex: wrong BP reg id for address (0x4A instead of 0x4B)",
     "    reg = set_field(reg, 8, 24, 0x4Bu);",
     "    reg = set_field(reg, 8, 24, 0x4Au); // MUTANT",
     1505, 1),  # in GXCopyTex (~1515)
    # 15
    ("gx_draw_done_decrement",
     "gx_draw_done",
     "GXDrawDone: decrement instead of increment",
     "    gc_gx_draw_done_calls++;",
     "    gc_gx_draw_done_calls--; // MUTANT",
     0, 1),
    # 16
    ("gx_enable_tex_offsets_swap_bits",
     "gx_enable_tex_offsets",
     "GXEnableTexOffsets: use bit 19 for line instead of 18",
     "    gc_gx_su_ts0[coord] = set_field(gc_gx_su_ts0[coord], 1, 18, (u32)(line_enable != 0));",
     "    gc_gx_su_ts0[coord] = set_field(gc_gx_su_ts0[coord], 1, 19, (u32)(line_enable != 0)); // MUTANT",
     0, 1),
    # 17
    ("gx_get_tex_buffer_size_wrong_tile",
     "gx_get_tex_buffer_size",
     "GXGetTexBufferSize: wrong tileBytes for non-RGBA8 (64 instead of 32)",
     "    tileBytes = ((format & 0x3Fu) == GX_TF_RGBA8) ? 64u : 32u;",
     "    tileBytes = ((format & 0x3Fu) == GX_TF_RGBA8) ? 64u : 64u; // MUTANT",
     0, 1),
    # 18
    ("gx_get_y_scale_factor_wrong_cmp",
     "gx_get_y_scale_factor",
     "GXGetYScaleFactor: flip first while comparison",
     "    while (realHt > xfbHeight) {",
     "    while (realHt < xfbHeight) { // MUTANT",
     1020, 1),  # first occurrence in GXGetYScaleFactor (~1027)
    # 19
    ("gx_init_wrong_bp_mask",
     "gx_init",
     "GXInit: wrong bp_mask default (0xFE instead of 0xFF)",
     "    gc_gx_bp_mask = 0xFF;",
     "    gc_gx_bp_mask = 0xFE; // MUTANT",
     0, 1),
    # 20
    ("gx_init_light_dist_attn_flip_cmp",
     "gx_init_light_dist_attn",
     "GXInitLightDistAttn: flip ref_dist comparison",
     "    if (ref_dist < 0.0f) {",
     "    if (ref_dist > 0.0f) { // MUTANT",
     0, 1),
    # 21
    ("gx_init_light_spot_wrong_cutoff",
     "gx_init_light_spot",
     "GXInitLightSpot: change cutoff limit from 90 to 80",
     "    if (cutoff <= 0.0f || cutoff > 90.0f) {",
     "    if (cutoff <= 0.0f || cutoff > 80.0f) { // MUTANT",
     0, 1),
    # 22
    ("gx_init_specular_dir_remove_offset",
     "gx_init_specular_dir",
     "GXInitSpecularDir: remove +1.0 offset on vz",
     "    vz = -nz + 1.0f;",
     "    vz = -nz; // MUTANT",
     0, 1),
    # 23
    ("gx_init_tex_obj_wrong_wrap_shift",
     "gx_init_tex_obj",
     "GXInitTexObj: wrong bit position for wrap_s (2 instead of 0)",
     "    t->mode0 = set_field(t->mode0, 2, 0, wrap_s);",
     "    t->mode0 = set_field(t->mode0, 2, 2, wrap_s); // MUTANT",
     0, 1),
    # 24
    ("gx_init_tex_obj_lod_wrong_scale",
     "gx_init_tex_obj_lod",
     "GXInitTexObjLOD: wrong lod_bias scale factor (16 instead of 32)",
     "    u8 lbias = (u8)(32.0f * lod_bias);",
     "    u8 lbias = (u8)(16.0f * lod_bias); // MUTANT",
     0, 1),
    # 25
    ("gx_invalidate_tex_all_decrement",
     "gx_invalidate_tex_all",
     "GXInvalidateTexAll: decrement instead of increment",
     "    gc_gx_invalidate_tex_all_calls++;",
     "    gc_gx_invalidate_tex_all_calls--; // MUTANT",
     0, 1),
    # 26
    ("gx_invalidate_vtx_cache_decrement",
     "gx_invalidate_vtx_cache",
     "GXInvalidateVtxCache: decrement instead of increment",
     "    gc_gx_invalidate_vtx_cache_calls++;",
     "    gc_gx_invalidate_vtx_cache_calls--; // MUTANT",
     0, 1),
    # 27
    ("gx_load_light_obj_imm_and_mask",
     "gx_load_light_obj_imm",
     "GXLoadLightObjImm: use AND instead of OR for mask",
     "    gc_gx_light_loaded_mask |= (1u << idx);",
     "    gc_gx_light_loaded_mask &= (1u << idx); // MUTANT",
     0, 1),
    # 28
    ("gx_load_nrm_mtx_imm_wrong_mult",
     "gx_load_nrm_mtx_imm",
     "GXLoadNrmMtxImm: wrong addr multiplier (4 instead of 3)",
     "    const u32 addr = id * 3u + 0x400u;",
     "    const u32 addr = id * 4u + 0x400u; // MUTANT",
     0, 1),
    # 29
    ("gx_load_pos_mtx_imm_wrong_mask",
     "gx_load_pos_mtx_imm",
     "GXLoadPosMtxImm: wrong register mask (0xC0000 instead of 0xB0000)",
     "    const u32 reg = addr | 0xB0000u;",
     "    const u32 reg = addr | 0xC0000u; // MUTANT",
     0, 1),
    # 30
    ("gx_load_tex_mtx_imm_wrong_offset",
     "gx_load_tex_mtx_imm",
     "GXLoadTexMtxImm: wrong post-transform base (0x400 instead of 0x500)",
     "        addr = (id - 64u) * 4u + 0x500u;",
     "        addr = (id - 64u) * 4u + 0x400u; // MUTANT",
     0, 1),
    # 31
    ("gx_load_tex_obj_skip_cb",
     "gx_load_tex_obj",
     "GXLoadTexObj: always use region 0 instead of callback",
     "    GXTexRegion *r = gc_gx_tex_region_cb(obj, id);",
     "    GXTexRegion *r = &gc_gx_tex_regions[0]; // MUTANT",
     0, 1),
    # 32
    ("gx_poke_alpha_mode_swap",
     "gx_poke_alpha_mode",
     "GXPokeAlphaMode: store threshold into func field",
     "    gc_gx_poke_alpha_mode_func = func;",
     "    gc_gx_poke_alpha_mode_func = (u32)threshold; // MUTANT",
     0, 1),
    # 33
    ("gx_poke_alpha_read_zero",
     "gx_poke_alpha_read",
     "GXPokeAlphaRead: always store 0",
     "    gc_gx_poke_alpha_read_mode = mode;",
     "    gc_gx_poke_alpha_read_mode = 0; // MUTANT",
     0, 1),
    # 34
    ("gx_poke_alpha_update_negate",
     "gx_poke_alpha_update",
     "GXPokeAlphaUpdate: negate enable flag",
     "    gc_gx_poke_alpha_update_enable = (u32)enable;",
     "    gc_gx_poke_alpha_update_enable = (u32)!enable; // MUTANT",
     0, 1),
    # 35
    ("gx_poke_blend_mode_swap_src_dst",
     "gx_poke_blend_mode",
     "GXPokeBlendMode: store dst_factor into src field",
     "    gc_gx_poke_blend_src = src_factor;",
     "    gc_gx_poke_blend_src = dst_factor; // MUTANT",
     0, 1),
    # 36
    ("gx_poke_color_update_negate",
     "gx_poke_color_update",
     "GXPokeColorUpdate: negate enable flag",
     "    gc_gx_poke_color_update_enable = (u32)enable;",
     "    gc_gx_poke_color_update_enable = (u32)!enable; // MUTANT",
     0, 1),
    # 37
    ("gx_poke_dither_negate",
     "gx_poke_dither",
     "GXPokeDither: negate enable flag",
     "    gc_gx_poke_dither_enable = (u32)enable;",
     "    gc_gx_poke_dither_enable = (u32)!enable; // MUTANT",
     0, 1),
    # 38
    ("gx_poke_dst_alpha_swap",
     "gx_poke_dst_alpha",
     "GXPokeDstAlpha: store alpha into enable field",
     "    gc_gx_poke_dst_alpha_enable = (u32)enable;",
     "    gc_gx_poke_dst_alpha_enable = (u32)alpha; // MUTANT",
     0, 1),
    # 39
    ("gx_poke_z_mode_swap",
     "gx_poke_z_mode",
     "GXPokeZMode: store update_enable into enable field",
     "    gc_gx_poke_zmode_enable = (u32)enable;",
     "    gc_gx_poke_zmode_enable = (u32)update_enable; // MUTANT",
     0, 1),
    # 40
    ("gx_position_1x16_zero",
     "gx_position_1x16",
     "GXPosition1x16: always store 0",
     "    gc_gx_pos1x16_last = (u32)x;",
     "    gc_gx_pos1x16_last = 0; // MUTANT",
     0, 1),
    # 41
    ("gx_position_2f32_swap_xy",
     "gx_position_2f32",
     "GXPosition2f32: swap x and y stores",
     "    gc_gx_pos2f32_x_bits = f32_bits(x);",
     "    gc_gx_pos2f32_x_bits = f32_bits(y); // MUTANT",
     0, 1),
    # 42
    ("gx_position_2s16_no_sign_ext",
     "gx_position_2s16",
     "GXPosition2s16: zero-extend x instead of sign-extend",
     "    gc_gx_pos2s16_x = (u32)(s32)x;",
     "    gc_gx_pos2s16_x = (u32)(u16)x; // MUTANT",
     0, 1),
    # 43
    ("gx_position_2u16_swap",
     "gx_position_2u16",
     "GXPosition2u16: swap x and y stores",
     "    gc_gx_pos2u16_x = (u32)x;",
     "    gc_gx_pos2u16_x = (u32)y; // MUTANT",
     0, 1),
    # 44
    ("gx_position_3f32_swap_xz",
     "gx_position_3f32",
     "GXPosition3f32: swap x and z stores",
     "    gc_gx_pos3f32_x_bits = f32_bits(x);",
     "    gc_gx_pos3f32_x_bits = f32_bits(z); // MUTANT",
     0, 1),
    # 45
    ("gx_position_3s16_swap_xz",
     "gx_position_3s16",
     "GXPosition3s16: swap x and z stores",
     "    gc_gx_pos3s16_x = (u32)(s32)x;",
     "    gc_gx_pos3s16_x = (u32)(s32)z; // MUTANT",
     0, 1),
    # 46
    ("gx_read_gp_metric_swap",
     "gx_read_gp_metric",
     "GXReadGPMetric: swap perf0/perf1 outputs",
     "    if (met0) *met0 = gc_gx_gp_perf0;",
     "    if (met0) *met0 = gc_gx_gp_perf1; // MUTANT",
     0, 1),
    # 47
    ("gx_read_mem_metric_swap",
     "gx_read_mem_metric",
     "GXReadMemMetric: swap cp_req and tc_req",
     "    if (cp_req) *cp_req = gc_gx_mem_metrics[0];",
     "    if (cp_req) *cp_req = gc_gx_mem_metrics[1]; // MUTANT",
     0, 1),
    # 48
    ("gx_read_pix_metric_wrong_idx",
     "gx_read_pix_metric",
     "GXReadPixMetric: wrong index for top_out (2 instead of 1)",
     "    if (top_out) *top_out = gc_gx_pix_metrics[1];",
     "    if (top_out) *top_out = gc_gx_pix_metrics[2]; // MUTANT",
     0, 1),
    # 49
    ("gx_read_vcache_metric_wrong_miss",
     "gx_read_vcache_metric",
     "GXReadVCacheMetric: return 1 for miss instead of 0",
     "    if (miss) *miss = 0;",
     "    if (miss) *miss = 1; // MUTANT",
     0, 1),
    # 50
    ("gx_set_alpha_compare_wrong_shift",
     "gx_set_alpha_compare",
     "GXSetAlphaCompare: wrong ref1 bit position (0 instead of 8)",
     "    reg = set_field(reg, 8, 8, (u32)ref1);",
     "    reg = set_field(reg, 8, 0, (u32)ref1); // MUTANT",
     2010, 1),  # in GXSetAlphaCompare (~2015)
    # 51
    ("gx_set_alpha_update_wrong_bit",
     "gx_set_alpha_update",
     "GXSetAlphaUpdate: wrong cmode0 bit (3 instead of 4)",
     "    gc_gx_cmode0 = set_field(gc_gx_cmode0, 1, 4, (u32)(update_enable != 0));",
     "    gc_gx_cmode0 = set_field(gc_gx_cmode0, 1, 3, (u32)(update_enable != 0)); // MUTANT",
     0, 1),
    # 52
    ("gx_set_array_wrong_offset",
     "gx_set_array",
     "GXSetArray: use raw attr instead of attr-GX_VA_POS",
     "    u32 cp_attr = attr - GX_VA_POS;",
     "    u32 cp_attr = attr; // MUTANT",
     0, 1),
    # 53
    ("gx_set_blend_mode_wrong_bit",
     "gx_set_blend_mode",
     "GXSetBlendMode: wrong cmode0 bit for blend_enable (1 instead of 0)",
     "    gc_gx_cmode0 = set_field(gc_gx_cmode0, 1, 0, blend_enable);",
     "    gc_gx_cmode0 = set_field(gc_gx_cmode0, 1, 1, blend_enable); // MUTANT",
     0, 1),
    # 54
    ("gx_set_chan_amb_color_wrong_xf",
     "gx_set_chan_amb_color",
     "GXSetChanAmbColor: wrong XF reg offset (11 instead of 10)",
     "    gx_write_xf_reg(colIdx + 10, reg);",
     "    gx_write_xf_reg(colIdx + 11, reg); // MUTANT",
     0, 1),
    # 55
    ("gx_set_chan_ctrl_wrong_xf",
     "gx_set_chan_ctrl",
     "GXSetChanCtrl: wrong XF reg base (15 instead of 14)",
     "    gx_write_xf_reg(idx + 14, reg);",
     "    gx_write_xf_reg(idx + 15, reg); // MUTANT",
     0, 1),
    # 56
    ("gx_set_chan_mat_color_wrong_xf",
     "gx_set_chan_mat_color",
     "GXSetChanMatColor: wrong XF reg offset (13 instead of 12)",
     "    gx_write_xf_reg(colIdx + 12, reg);",
     "    gx_write_xf_reg(colIdx + 13, reg); // MUTANT",
     0, 1),
    # 57
    ("gx_set_clip_mode_wrong_bp",
     "gx_set_clip_mode",
     "GXSetClipMode: wrong bp_sent_not value (0 instead of 1)",
     "    gc_gx_bp_sent_not = 1;",
     "    gc_gx_bp_sent_not = 0; // MUTANT",
     954, 1),  # in GXSetClipMode (~958)
    # 58
    ("gx_set_co_planar_wrong_bit",
     "gx_set_co_planar",
     "GXSetCoPlanar: wrong genMode bit (18 instead of 19)",
     "    gc_gx_gen_mode = set_field(gc_gx_gen_mode, 1, 19, (u32)(enable != 0));",
     "    gc_gx_gen_mode = set_field(gc_gx_gen_mode, 1, 18, (u32)(enable != 0)); // MUTANT",
     0, 1),
    # 59
    ("gx_set_color_update_wrong_bit",
     "gx_set_color_update",
     "GXSetColorUpdate: wrong cmode0 bit (2 instead of 3)",
     "    gc_gx_cmode0 = set_field(gc_gx_cmode0, 1, 3, (u32)enable);",
     "    gc_gx_cmode0 = set_field(gc_gx_cmode0, 1, 2, (u32)enable); // MUTANT",
     1796, 1),  # in GXSetColorUpdate (~1800)
    # 60
    ("gx_set_copy_clamp_zero",
     "gx_set_copy_clamp",
     "GXSetCopyClamp: always store 0",
     "    gc_gx_copy_clamp = clamp;",
     "    gc_gx_copy_clamp = 0; // MUTANT",
     0, 1),
    # 61
    ("gx_set_copy_clear_wrong_reg",
     "gx_set_copy_clear",
     "GXSetCopyClear: wrong BP reg id for first reg (0x50 instead of 0x4F)",
     "    reg = set_field(reg, 8, 24, 0x4Fu);",
     "    reg = set_field(reg, 8, 24, 0x50u); // MUTANT",
     1597, 1),  # in GXSetCopyClear (~1605)
    # 62
    ("gx_set_copy_filter_swap_aa_vf",
     "gx_set_copy_filter",
     "GXSetCopyFilter: store vf into aa field",
     "    gc_gx_copy_filter_aa = (u32)aa;",
     "    gc_gx_copy_filter_aa = (u32)vf; // MUTANT",
     0, 1),
    # 63
    ("gx_set_cull_mode_no_swap",
     "gx_set_cull_mode",
     "GXSetCullMode: remove front/back HW swap",
     "    if (mode == 1u) hwMode = 2u;",
     "    if (mode == 1u) hwMode = 1u; // MUTANT",
     0, 1),
    # 64
    ("gx_set_current_mtx_wrong_size",
     "gx_set_current_mtx",
     "GXSetCurrentMtx: wrong field size (5 instead of 6)",
     "    gc_gx_mat_idx_a = set_field(gc_gx_mat_idx_a, 6, 0, id);",
     "    gc_gx_mat_idx_a = set_field(gc_gx_mat_idx_a, 5, 0, id); // MUTANT",
     1624, 1),  # in GXSetCurrentMtx (~1628)
    # 65
    ("gx_set_disp_copy_dst_wrong_stride",
     "gx_set_disp_copy_dst",
     "GXSetDispCopyDst: wrong stride multiplier (3 instead of 2)",
     "    u16 stride = (u16)(wd * 2u);",
     "    u16 stride = (u16)(wd * 3u); // MUTANT",
     0, 1),
    # 66
    ("gx_set_disp_copy_frame2field_zero",
     "gx_set_disp_copy_frame2field",
     "GXSetDispCopyFrame2Field: always store 0",
     "    gc_gx_copy_frame2field = mode;",
     "    gc_gx_copy_frame2field = 0; // MUTANT",
     0, 1),
    # 67
    ("gx_set_disp_copy_gamma_zero",
     "gx_set_disp_copy_gamma",
     "GXSetDispCopyGamma: always store 0",
     "    gc_gx_copy_gamma = gamma;",
     "    gc_gx_copy_gamma = 0; // MUTANT",
     0, 1),
    # 68
    ("gx_set_disp_copy_src_wrong_reg",
     "gx_set_disp_copy_src",
     "GXSetDispCopySrc: wrong BP reg id (0x4B instead of 0x49)",
     "    reg = set_field(reg, 8, 24, 0x49);",
     "    reg = set_field(reg, 8, 24, 0x4B); // MUTANT",
     966, 1),  # in GXSetDispCopySrc (~971)
    # 69
    ("gx_set_disp_copy_y_scale_wrong_mask",
     "gx_set_disp_copy_y_scale",
     "GXSetDispCopyYScale: wrong scale mask (0x1FE instead of 0x1FF)",
     "    u32 scale = ((u32)(256.0f / vscale)) & 0x1FFu;",
     "    u32 scale = ((u32)(256.0f / vscale)) & 0x1FEu; // MUTANT",
     1006, 1),  # in GXSetDispCopyYScale (~1008)
    # 70
    ("gx_set_dither_wrong_bit",
     "gx_set_dither",
     "GXSetDither: wrong cmode0 bit (3 instead of 2)",
     "    gc_gx_cmode0 = set_field(gc_gx_cmode0, 1, 2, (u32)(dither != 0));",
     "    gc_gx_cmode0 = set_field(gc_gx_cmode0, 1, 3, (u32)(dither != 0)); // MUTANT",
     0, 1),
    # 71
    ("gx_set_draw_done_wrong_magic",
     "gx_set_draw_done",
     "GXSetDrawDone: wrong RAS magic constant",
     "    gx_write_ras_reg(0x45000002u);",
     "    gx_write_ras_reg(0x45000001u); // MUTANT",
     0, 1),
    # 72
    ("gx_set_draw_sync_wrong_high",
     "gx_set_draw_sync",
     "GXSetDrawSync: wrong high byte (0x47 instead of 0x48)",
     "    u32 reg = ((u32)token) | 0x48000000u;",
     "    u32 reg = ((u32)token) | 0x47000000u; // MUTANT",
     0, 1),
    # 73
    ("gx_set_draw_sync_callback_null_ret",
     "gx_set_draw_sync_callback",
     "GXSetDrawSyncCallback: return NULL instead of old callback",
     "    GXDrawSyncCallback old = (GXDrawSyncCallback)gc_gx_token_cb_ptr;",
     "    GXDrawSyncCallback old = (GXDrawSyncCallback)0; // MUTANT",
     0, 1),
    # 74
    ("gx_set_dst_alpha_swap",
     "gx_set_dst_alpha",
     "GXSetDstAlpha: store alpha into enable field",
     "    gc_gx_dst_alpha_enable = (u32)enable;",
     "    gc_gx_dst_alpha_enable = (u32)alpha; // MUTANT",
     1113, 1),  # in GXSetDstAlpha (~1115)
    # 75
    ("gx_set_field_mask_swap",
     "gx_set_field_mask",
     "GXSetFieldMask: store odd into even field",
     "    gc_gx_field_mask_even = (u32)even;",
     "    gc_gx_field_mask_even = (u32)odd; // MUTANT",
     0, 1),
    # 76
    ("gx_set_field_mode_swap",
     "gx_set_field_mode",
     "GXSetFieldMode: store half_aspect into field_mode field",
     "    gc_gx_field_mode_field_mode = (u32)field_mode;",
     "    gc_gx_field_mode_field_mode = (u32)half_aspect; // MUTANT",
     0, 1),
    # 77
    ("gx_set_fog_wrong_reg",
     "gx_set_fog",
     "GXSetFog: wrong BP reg id for fog0 (0xEF instead of 0xEE)",
     "    fog0 = set_field(fog0, 8, 24, 0xEEu);",
     "    fog0 = set_field(fog0, 8, 24, 0xEFu); // MUTANT",
     0, 1),
    # 78
    ("gx_set_gp_metric_swap",
     "gx_set_gp_metric",
     "GXSetGPMetric: store perf1 into perf0 field",
     "    gc_gx_gp_perf0 = perf0;",
     "    gc_gx_gp_perf0 = perf1; // MUTANT",
     2061, 1),  # in GXSetGPMetric (~2063)
    # 79
    ("gx_set_line_width_wrong_shift",
     "gx_set_line_width",
     "GXSetLineWidth: wrong bit position for width (8 instead of 0)",
     "    gc_gx_lp_size = set_field(gc_gx_lp_size, 8, 0, (u32)width);",
     "    gc_gx_lp_size = set_field(gc_gx_lp_size, 8, 8, (u32)width); // MUTANT",
     0, 1),
    # 80
    ("gx_set_num_chans_wrong_shift",
     "gx_set_num_chans",
     "GXSetNumChans: wrong genMode bit position (0 instead of 4)",
     "    gc_gx_gen_mode = set_field(gc_gx_gen_mode, 3, 4, (u32)nChans);",
     "    gc_gx_gen_mode = set_field(gc_gx_gen_mode, 3, 0, (u32)nChans); // MUTANT",
     0, 1),
    # 81
    ("gx_set_num_tev_stages_no_sub",
     "gx_set_num_tev_stages",
     "GXSetNumTevStages: store nStages without -1",
     "    gc_gx_gen_mode = set_field(gc_gx_gen_mode, 4, 10, (u32)(nStages - 1u));",
     "    gc_gx_gen_mode = set_field(gc_gx_gen_mode, 4, 10, (u32)(nStages)); // MUTANT",
     0, 1),
    # 82
    ("gx_set_num_tex_gens_wrong_size",
     "gx_set_num_tex_gens",
     "GXSetNumTexGens: wrong genMode field size (3 instead of 4)",
     "    gc_gx_gen_mode = set_field(gc_gx_gen_mode, 4, 0, (u32)nTexGens);",
     "    gc_gx_gen_mode = set_field(gc_gx_gen_mode, 3, 0, (u32)nTexGens); // MUTANT",
     0, 1),
    # 83
    ("gx_set_pixel_fmt_swap",
     "gx_set_pixel_fmt",
     "GXSetPixelFmt: store z_fmt into pixel_fmt field",
     "    gc_gx_pixel_fmt = pix_fmt;",
     "    gc_gx_pixel_fmt = z_fmt; // MUTANT",
     0, 1),
    # 84
    ("gx_set_point_size_wrong_shift",
     "gx_set_point_size",
     "GXSetPointSize: wrong bit position for pointSize (0 instead of 8)",
     "    gc_gx_lp_size = set_field(gc_gx_lp_size, 8, 8, (u32)pointSize);",
     "    gc_gx_lp_size = set_field(gc_gx_lp_size, 8, 0, (u32)pointSize); // MUTANT",
     0, 1),
    # 85
    ("gx_set_projection_swap_type",
     "gx_set_projection",
     "GXSetProjection: swap ortho/perspective element picks",
     "        p1 = mtx[0][3];",
     "        p1 = mtx[0][2]; // MUTANT",
     1753, 1),  # in GXSetProjection (~1765)
    # 86
    ("gx_set_scissor_wrong_offset",
     "gx_set_scissor",
     "GXSetScissor: wrong offset (341 instead of 342)",
     "    u32 tp = top + 342u;",
     "    u32 tp = top + 341u; // MUTANT",
     0, 1),
    # 87
    ("gx_set_scissor_box_offset_wrong_shift",
     "gx_set_scissor_box_offset",
     "GXSetScissorBoxOffset: wrong shift (2 instead of 1)",
     "    u32 hx = (u32)(x_off + 342) >> 1;",
     "    u32 hx = (u32)(x_off + 342) >> 2; // MUTANT",
     0, 1),
    # 88
    ("gx_set_tev_alpha_in_wrong_size",
     "gx_set_tev_alpha_in",
     "GXSetTevAlphaIn: wrong field size for a (4 instead of 3)",
     "    reg = set_field(reg, 3, 13, a);",
     "    reg = set_field(reg, 4, 13, a); // MUTANT",
     2356, 1),  # in GXSetTevAlphaIn (~2360)
    # 89
    ("gx_set_tev_alpha_op_wrong_pos",
     "gx_set_tev_alpha_op",
     "GXSetTevAlphaOp: wrong field position for op (19 instead of 18)",
     "    reg = set_field(reg, 1, 18, op & 1u);",
     "    reg = set_field(reg, 1, 19, op & 1u); // MUTANT",
     2386, 1),  # in GXSetTevAlphaOp (~2390)
    # 90
    ("gx_set_tev_color_wrong_id",
     "gx_set_tev_color",
     "GXSetTevColor: wrong reg id stride (3 instead of 2)",
     "    regRA = set_field(regRA, 8, 24, (u32)(224u + id * 2u));",
     "    regRA = set_field(regRA, 8, 24, (u32)(224u + id * 3u)); // MUTANT",
     0, 1),
    # 91
    ("gx_set_tev_color_in_wrong_pos",
     "gx_set_tev_color_in",
     "GXSetTevColorIn: wrong field position for b (4 instead of 8)",
     "    reg = set_field(reg, 4, 8, b);",
     "    reg = set_field(reg, 4, 4, b); // MUTANT",
     2344, 1),  # in GXSetTevColorIn (~2349)
    # 92
    ("gx_set_tev_color_op_wrong_pos",
     "gx_set_tev_color_op",
     "GXSetTevColorOp: wrong field position for op (19 instead of 18)",
     "    reg = set_field(reg, 1, 18, op & 1u);",
     "    reg = set_field(reg, 1, 19, op & 1u); // MUTANT",
     2368, 1),  # in GXSetTevColorOp (~2372)
    # 93
    ("gx_set_tev_op_wrong_carg",
     "gx_set_tev_op",
     "GXSetTevOp: wrong color arg for MODULATE (9 instead of 8)",
     "        GXSetTevColorIn(id, 0u, 8u, carg, 0u);",
     "        GXSetTevColorIn(id, 0u, 9u, carg, 0u); // MUTANT",
     0, 1),
    # 94
    ("gx_set_tev_order_wrong_shift",
     "gx_set_tev_order",
     "GXSetTevOrder: wrong tref tmap field position for even stage (1 instead of 0)",
     "        *ptref = set_field(*ptref, 3, 0, tmap);",
     "        *ptref = set_field(*ptref, 3, 1, tmap); // MUTANT",
     0, 1),
    # 95
    ("gx_set_tex_coord_gen_wrong_postmtx",
     "gx_set_tex_coord_gen",
     "GXSetTexCoordGen: wrong default postmtx (124 instead of 125)",
     "    GXSetTexCoordGen2(dst_coord, func, src_param, mtx, 0, 125u);",
     "    GXSetTexCoordGen2(dst_coord, func, src_param, mtx, 0, 124u); // MUTANT",
     0, 1),
    # 96
    ("gx_set_tex_copy_dst_wrong_field",
     "gx_set_tex_copy_dst",
     "GXSetTexCopyDst: wrong cpTex field 15 value (1 instead of 2)",
     "    gc_gx_cp_tex = set_field(gc_gx_cp_tex, 2, 15, 2u);",
     "    gc_gx_cp_tex = set_field(gc_gx_cp_tex, 2, 15, 1u); // MUTANT",
     0, 1),
    # 97
    ("gx_set_tex_copy_src_no_minus1",
     "gx_set_tex_copy_src",
     "GXSetTexCopySrc: omit -1 from width",
     "    gc_gx_cp_tex_size = set_field(gc_gx_cp_tex_size, 10, 0, (u32)(wd - 1u));",
     "    gc_gx_cp_tex_size = set_field(gc_gx_cp_tex_size, 10, 0, (u32)(wd)); // MUTANT",
     0, 1),
    # 98
    ("gx_set_vcache_metric_zero",
     "gx_set_vcache_metric",
     "GXSetVCacheMetric: always store 0",
     "    gc_gx_vcache_sel = attr;",
     "    gc_gx_vcache_sel = 0; // MUTANT",
     0, 1),
    # 99
    ("gx_set_viewport_wrong_field",
     "gx_set_viewport",
     "GXSetViewport: pass field=0 instead of 1",
     "    GXSetViewportJitter(left, top, wd, ht, nearz, farz, 1u);",
     "    GXSetViewportJitter(left, top, wd, ht, nearz, farz, 0u); // MUTANT",
     0, 1),
    # 100
    ("gx_set_viewport_jitter_wrong_cmp",
     "gx_set_viewport_jitter",
     "GXSetViewportJitter: flip jitter comparison (1 instead of 0)",
     "    if (field == 0) top -= 0.5f;",
     "    if (field == 1) top -= 0.5f; // MUTANT",
     0, 1),
    # 101
    ("gx_set_vtx_attr_fmt_wrong_dirty",
     "gx_set_vtx_attr_fmt",
     "GXSetVtxAttrFmt: wrong dirty_state bit (0x08 instead of 0x10)",
     "    gc_gx_dirty_state |= 0x10u;",
     "    gc_gx_dirty_state |= 0x08u; // MUTANT",
     0, 1),
    # 102
    ("gx_set_vtx_desc_wrong_dirty",
     "gx_set_vtx_desc",
     "GXSetVtxDesc: wrong dirty_state bit (4 instead of 8)",
     "    gc_gx_dirty_state |= 8u;",
     "    gc_gx_dirty_state |= 4u; // MUTANT",
     2183, 1),  # in GXSetVtxDesc (~2191)
    # 103
    ("gx_set_z_comp_loc_wrong_bit",
     "gx_set_z_comp_loc",
     "GXSetZCompLoc: wrong peCtrl bit (7 instead of 6)",
     "    gc_gx_pe_ctrl = set_field(gc_gx_pe_ctrl, 1, 6, (u32)(before_tex != 0));",
     "    gc_gx_pe_ctrl = set_field(gc_gx_pe_ctrl, 1, 7, (u32)(before_tex != 0)); // MUTANT",
     0, 1),
    # 104
    ("gx_set_z_mode_wrong_shift",
     "gx_set_z_mode",
     "GXSetZMode: wrong zmode bit position for func (0 instead of 1)",
     "    gc_gx_zmode = set_field(gc_gx_zmode, 3, 1, func & 7u);",
     "    gc_gx_zmode = set_field(gc_gx_zmode, 3, 0, func & 7u); // MUTANT",
     0, 1),
    # 105
    ("gx_tex_coord_2f32_swap_st",
     "gx_tex_coord_2f32",
     "GXTexCoord2f32: swap s and t stores",
     "    gc_gx_texcoord2f32_s_bits = f32_bits(s);",
     "    gc_gx_texcoord2f32_s_bits = f32_bits(t); // MUTANT",
     0, 1),
    # 106
    ("gx_wait_draw_done_wrong_flag",
     "gx_wait_draw_done",
     "GXWaitDrawDone: set draw_done_flag to 0 instead of 1",
     "    gc_gx_draw_done_flag = 1;",
     "    gc_gx_draw_done_flag = 0; // MUTANT",
     0, 1),
]


def main():
    errors = []
    count = 0

    for (patch_name, test_dir, desc, old_line, new_line, after_line, occurrence) in MUTATIONS:
        patch_content, err = gen_patch(old_line, new_line, after_line=after_line, occurrence=occurrence)
        if patch_content is None:
            errors.append(f"FAIL: {patch_name}: could not find line: {err}")
            continue

        patch_path = os.path.join(MUTATIONS_DIR, f"{patch_name}.patch")
        sh_path = os.path.join(MUTATIONS_DIR, f"{patch_name}.sh")

        with open(patch_path, "w", newline="\n") as f:
            f.write(patch_content)

        sh_content = gen_sh(patch_name, test_dir, desc)
        with open(sh_path, "w", newline="\n") as f:
            f.write(sh_content)

        count += 1

    if errors:
        for e in errors:
            print(e, file=sys.stderr)
        print(f"\nGenerated {count} mutation pairs. {len(errors)} FAILURES.", file=sys.stderr)
        sys.exit(1)
    else:
        print(f"Generated {count} mutation patches and {count} shell wrappers.")


if __name__ == "__main__":
    main()
