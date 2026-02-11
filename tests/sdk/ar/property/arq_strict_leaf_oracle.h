/*
 * arq_strict_leaf_oracle.h
 *
 * Strict leaf oracle facts from decomp ARQPostRequest:
 * if callback is NULL, SDK swaps it to __ARQCallbackHack, so the stored
 * callback pointer is always non-NULL.
 *
 * For our host parity, this means callback field normalization should always
 * become 1 in the compact model.
 */
#pragma once

static int strict_ARQNormalizeCallback(int has_callback) {
    (void)has_callback;
    return 1;
}
