/**
 * @file printer_tree.c
 * @author Adam Piecek <piecek@cesnet.cz>
 * @brief RFC tree printer for libyang data structure
 *
 * Copyright (c) 2015 - 2020 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "context.h"
#include "dict.h"
#include "log.h"
#include "parser_data.h"
#include "plugins_types.h"
#include "printer_internal.h"
#include "tree.h"
#include "tree_schema.h"

/* module: <name>
 * <X>+--rw <node-name> */
#define LY_TREE_MOD_DATA_INDENT 2

/* <^>rpcs:
 * <X>+---x <rpc-name> */
#define LY_TREE_OP_DATA_INDENT 4

/* +--rw leaf<X>string */
#define LY_TREE_TYPE_INDENT 3

/* +--rw leaf
 * |     <X>string */
#define LY_TREE_WRAP_INDENT 2

/* these options are mostly inherited in recursive print, non-recursive options are parameters */
struct tree_ctx {
    struct ly_out *out;              /**< output specification */
    const struct lys_module *module; /**< (sub)module we are printing from */
    uint8_t base_indent;             /**< base indent size of all the printed text */
    uint64_t indent;                 /**< bit-field of sibling (1)/ no sibling(0) on corresponding depths */
    uint16_t line_length;            /**< maximum desired line length */
    int spec_config;                 /**< special config flags - 0 (no special config status),
                                            1 (read-only - rpc output, notification), 2 (write-only - rpc input) */
    int options;                     /**< user-specified tree printer options */
};

/*
 Abbreviations for functions:
tree -> interface of tree printer
trp  -> function whose parameter is from parsed tree
gen  -> parameter data types are simple or defined in this file
*/

static int
gen_print_indent(struct ly_out *out, uint8_t base_indent, uint64_t indent, int level)
{
    int i, ret = 0;

    if (base_indent) {
        ret += ly_print_(out, "%*s", base_indent, " ");
    }
    for (i = 0; i < level; ++i) {
        if (indent & (1 << i)) {
            ret += ly_print_(out, "|  ");
        } else {
            ret += ly_print_(out, "   ");
        }
    }

    return ret;
}

static int
trp_print_config(struct ly_out *out, int spec_config, uint16_t UNUSED(nodetype), uint16_t nodeflags)
{
    int ret;

    /* Skipped actions: */
    /* for nodetype LYS_RPC, LYS_ACTION, LYS_NOTIF, LYS_USES, LYS_CASE */

    if (spec_config == 1) {
        ret = ly_print_(out, "-w ");
    } else if (spec_config == 2) {
        ret = ly_print_(out, "ro ");
    } else {
        /* "rw" is the default output unless a closer specification is set */
        ret = ly_print_(out, "%s ", (nodeflags & LYS_CONFIG_W) ? "rw" : (nodeflags & LYS_CONFIG_R) ? "ro" : "rw");
    }

    /* Skipped actions: */
    /* if (nodetype == LYS_CHOICE) { */

    return ret;
}

static int
trp_print_prefix(struct ly_out *UNUSED(out), const struct lysp_node *UNUSED(node), struct tree_ctx *UNUSED(ctx))
{
    uint16_t ret = 0;

    /* Skipped actions: */
    /* nodemod = lys_node_module(node); */
    /* if (lys_main_module(opts->module) != nodemod) { */
    /*      if (opts->options & LYS_OUTOPT_TREE_RFC) { */

    return ret;
}

static void
trp_print_node(struct tree_ctx *ctx, const struct lysp_node *node, int level, int subtree)
{
    struct ly_out *out = ctx->out;
    int line_len;
    int node_len;

    /* Skipped actions... */
    /* disabled/not printed node */
    /* implicit input/output/case */
    /* special uses and grouping handling */

    /* print indent */
    line_len = gen_print_indent(out, ctx->base_indent, ctx->indent, level);
    /* print status */
    line_len += ly_print_(out, "%s--", (node->flags & LYS_STATUS_DEPRC ? "x" : (node->flags & LYS_STATUS_OBSLT ? "o" : "+")));
    /* print config flags (or special opening for case, choice) */
    line_len += trp_print_config(out, ctx->spec_config, node->nodetype, node->flags);
    /* print optionally prefix */
    node_len = trp_print_prefix(out, node, ctx);
    /* print name */
    node_len += ly_print_(out, node->name);

    /* print one-character opts */
    /* Skipped actions... */
    /* switch (node->nodetype & mask) { */

    line_len += node_len;

    /* learn next level indent (there is never a sibling for subtree) */
    ++level;
    if (!subtree) {
        // tree_next_indent(level, node, aug_parent, opts);
    }

    /* print type/keys */
    /* Skipped actions... */
    /* switch (node->nodetype & mask) { */

    /* print default */
    /* Skipped actions... */
    /* if (!(opts->options & LYS_OUTOPT_TREE_RFC)) { */

    /* print if-features */
    /* Skipped actions... */
    /* switch (node->nodetype & mask) { */

    /* this node is finished printing */
    ly_print_(out, "\n");

    /* we are printing subtree parents, finish here (or uses option) */
    /* Skipped actions.. */
    /* if ((subtree == 1) || ((node->nodetype & mask) == LYS_USES)) { */

    /* set special config flag */
    /* Skipped actions.. */
    /* switch (node->nodetype & mask) { */

    /* print_children */
    /* Skipped actions.. */
    /* set child mask and learn the longest child name (needed only if a child can have type) */
    /* switch (node->nodetype & mask) { */

    /* print descendants (children) */
    /* Skipped actions.. */
    /* if (child_mask) { */

    /* reset special config flag */
    /* Skipped actions.. */
    /* switch (node->nodetype & mask) { */

}

static void
trp_print_body(struct tree_ctx *ctx, const struct lysp_module *modp)
{
    struct lysp_node *node;

    ctx->base_indent = LY_TREE_MOD_DATA_INDENT;
    // mask = LYS_CHOICE | LYS_CONTAINER | LYS_LEAF | LYS_LEAFLIST | LYS_LIST | LYS_ANYDATA | LYS_USES;
    // max_child_len = tree_get_max_name_len(data, NULL, mask, &opts);

    LY_LIST_FOR(modp->data, node) {
        /* TODO: we're printing the submodule only? */

        switch(node->nodetype) {
            /* TODO: not ignore? */
            case LYS_RPC: break;
            case LYS_NOTIF: break;
            case LYS_GROUPING: break;
            default: 
            trp_print_node(ctx, node, 0, 0);
        }
    }

    /* all remaining nodes printed with operation indent */
    ctx->base_indent = LY_TREE_OP_DATA_INDENT;

    /* augments */
    /* TODO: augments actions */

    /* rpcs */
    /* TODO: rpcs actions */

    /* notifications */
    /* TODO: notifications actions */

    /* groupings */
    /* TODO: groupings actions */

}

LY_ERR tree_print_parsed_module(struct ly_out *out, const struct lys_module *module, const struct lysp_module *modp, uint32_t options)
{
    struct tree_ctx ctx_ = {.out = out, .module = module, .base_indent = 0, .line_length = 0, .spec_config = 0, .options = options}, *ctx = &ctx_;

    ly_print_(ctx->out, "Sorry, tree_print_parsed_module is not fully implemented\n");

    /* print module name */
    ly_print_(ctx->out, "module: %s\n", ctx->module->name);

    /* TODO: submodule? */
    /* TODO: path? (target_schema_path) */

    trp_print_body(ctx, modp);

    ly_print_flush(out);
    return LY_SUCCESS;
}

//LY_ERR tree_print_parsed_submodule(struct ly_out *out, const struct lys_module *module, const struct lysp_submodule *submodp, uint32_t options)
LY_ERR tree_print_parsed_submodule(struct ly_out *out, const struct lys_module *UNUSED(module), const struct lysp_submodule *UNUSED(submodp), uint32_t UNUSED(options))
{
    ly_print_(out, "Sorry, tree_print_parsed_submodule not implemented yet\n");
    return LY_SUCCESS;
}

//LY_ERR tree_print_compiled_node(struct ly_out *out, const struct lysc_node *node, uint32_t options)
LY_ERR tree_print_compiled_node(struct ly_out *out, const struct lysc_node *UNUSED(node), uint32_t UNUSED(options))
{
    ly_print_(out, "Sorry, tree_print_compiled_node not implemented yet\n");
    return LY_SUCCESS;
}
