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
#include "tree_schema_internal.h"

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
trp  -> function whose parameters is from parsed tree
trc  -> function whose parameters is from compiled tree
trb  -> function whose parameters is from both forms of tree - parsed and compiled
gen  -> parameter data types are simple or defined in this file
*/

#if 0
static int
trc_sibling_is_valid_child(const struct lysc_node *node, int including, const struct lys_module *UNUSED(module),
                            const struct lysc_node *aug_parent, uint16_t nodetype)
#endif

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
trc_print_config(struct ly_out *out, int spec_config, uint16_t nodetype, uint16_t nodeflags)
{
    int ret;

    /* Skipped actions: */
    /* for nodetype LYS_USES */
    switch (nodetype) {
    case LYS_RPC:
    case LYS_ACTION:
        return ly_print(out, "-x ");
    case LYS_NOTIF:
        return ly_print(out, "-n ");
    case LYS_CASE:
        return ly_print(out, ":(");
    default:
        break;
    }

    if (spec_config == 1) {
        ret = ly_print_(out, "-w ");
    } else if (spec_config == 2) {
        ret = ly_print_(out, "ro ");
    } else {
        /* "rw" is the default output unless a closer specification is set */
        ret = ly_print_(out, "%s ", (nodeflags & LYS_CONFIG_W) ? "rw" : (nodeflags & LYS_CONFIG_R) ? "ro" : "rw");
    }

    /* Skipped actions: */
    if (nodetype == LYS_CHOICE) {
        ret += ly_print(out, "(");
    }

    return ret;
}

static int
trc_print_prefix(struct ly_out *UNUSED(out), const struct lysc_node *UNUSED(node), struct tree_ctx *UNUSED(ctx))
{
    uint16_t ret = 0;

    /* Skipped actions: */
    /* nodemod = lys_node_module(node); */
    /* if (lys_main_module(opts->module) != nodemod) { */
    /*      if (opts->options & LYS_OUTOPT_TREE_RFC) { */

    return ret;
}

static int
trc_print_node_opts(struct ly_out *out, const struct lysc_node *node, uint16_t mask)
{
    int node_len = 0;
    /* print one-character opts */
    switch (node->nodetype & mask) {
    case LYS_LEAF:
        /* Skipped condition: && !tree_leaf_is_mandatory(node) */
        if (!(node->flags & LYS_MAND_TRUE)) {
            node_len += ly_print_(out, "?");
        }
        break;
    case LYS_ANYDATA:
    case LYS_ANYXML:
        if (!(node->flags & LYS_MAND_TRUE)) {
            node_len += ly_print_(out, "?");
        }
        break;
    case LYS_CONTAINER:
        if(node->sp && (((struct lysp_node_container *)node->sp)->presence)) {
            node_len += ly_print_(out, "!");
        }
        break;
    case LYS_LIST:
    case LYS_LEAFLIST:
        node_len += ly_print_(out, "*");
        break;
    case LYS_CASE:
        /* kinda shady, but consistent in a way */
        node_len += ly_print_(out, ")");
        break;
    case LYS_CHOICE:
        node_len += ly_print_(out, ")");
        if (!(node->flags & LYS_MAND_TRUE)) {
            node_len += ly_print(out, "?");
        }
        break;
    default:
        break;
    }
    return node_len;
}


static void
trc_next_indent(struct tree_ctx *ctx, const struct lysc_node *node, const struct lysc_node *UNUSED(aug_parent), int level)
{
    int next_is_case = 0, has_next = 0;

    if (level > 64) {
        LOGINT(node->module->ctx);
        return;
    }

    /* clear level indent (it may have been set for some line wrapping) */
    ctx->indent &= ~(uint64_t)(1ULL << (level - 1));

    /* this is the direct child of a case */
    if ((node->nodetype != LYS_CASE) && lysc_data_parent(node) && (lysc_data_parent(node)->nodetype & (LYS_CASE | LYS_CHOICE))) {
        /* it is not the only child */
        if (node->next && lysc_data_parent(node->next) && (lysc_data_parent(node->next)->nodetype == LYS_CHOICE)) {
            next_is_case = 1;
        }
    }

    /* next is a node that will actually be printed */
    /* Skipped action... */
    /* has_next = trc_sibling_is_valid_child(node, 0, ctx->module, aug_parent, node->nodetype); */
    /* Changed to: */
    has_next = node->next ? 1 : 0;

    /* set level indent */
    if (has_next && !next_is_case) {
        ctx->indent |= (uint64_t)1ULL << (level - 1);
    }
}


static int
trc_print_node_typekeys(struct tree_ctx *ctx, const struct lysc_node *node, uint16_t mask, int level, int line_len, int node_len, uint16_t max_name_len)
{
    //struct ly_out *out = ctx->out;
    //uint8_t text_indent, text_len;
    //const char *text_str;
    //switch (node->nodetype & mask) {
    //case LYS_LEAF:
    //case LYS_LEAFLIST:
    //    assert(max_name_len);
    //    text_indent = LY_TREE_TYPE_INDENT + (uint8_t)(max_name_len - node_len);
    //    text_len = tree_print_type(out, &((struct lysc_node_leaf *)node)->type, ctx->options, &text_str);
    //    line_len = tree_print_wrap(out, level, line_len, text_indent, text_len, ctx);
    //    line_len += ly_print(out, text_str);
    //    lydict_remove(ctx->module->ctx, text_str);
    //    break;
    //case LYS_ANYDATA:
    //    assert(max_name_len);
    //    text_indent = LY_TREE_TYPE_INDENT + (uint8_t)(max_name_len - node_len);
    //    line_len = tree_print_wrap(out, level, line_len, text_indent, 7, ctx);
    //    line_len += ly_print(out, "anydata");
    //    break;
    //case LYS_ANYXML:
    //    assert(max_name_len);
    //    text_indent = LY_TREE_TYPE_INDENT + (uint8_t)(max_name_len - node_len);
    //    line_len = tree_print_wrap(out, level, line_len, text_indent, 6, ctx);
    //    line_len += ly_print(out, "anyxml");
    //    break;
    //case LYS_LIST:
    //    text_len = tree_print_keys(out, ((struct lysc_node_list *)node)->keys, ((struct lysc_node_list *)node)->keys_size,
    //                               opts, &text_str);
    //    if (text_len) {
    //        line_len = tree_print_wrap(out, level, line_len, 1, text_len, opts);
    //        line_len += ly_print(out, text_str);
    //        lydict_remove(opts->module->ctx, text_str);
    //    }
    //    break;
    //default:
    //    break;
    //}

    return line_len;
}

static int
trc_print_node_default(struct tree_ctx *ctx, const struct lysc_node *node, uint16_t mask, int level, int line_len)
{
    //struct ly_out *out = ctx->out;
    //const char *text_str;
    //struct lysc_node *sub;

    //if (!(ctx->options & LYS_OUTOPT_TREE_RFC)) {
    //    switch (node->nodetype & mask) {
    //    case LYS_LEAF:
    //        text_str = ((struct lysc_node_leaf *)node)->dflt->canonical;
    //        if (text_str) {
    //            line_len = tree_print_wrap(out, level, line_len, 1, 2 + strlen(text_str), ctx);
    //            line_len += ly_print(out, "<%s>", text_str);
    //        }
    //        break;
    //    case LYS_CHOICE:
    //        sub = (struct lysc_node *) ((struct lysc_node_choice *)node)->dflt;
    //        if (sub) {
    //            line_len = tree_print_wrap(out, level, line_len, 1, 2 + strlen(sub->name), ctx);
    //            line_len += ly_print(out, "<%s>", sub->name);
    //        }
    //        break;
    //    default:
    //        break;
    //    }
    //}

    return line_len;
}

static int
trc_print_node_iffeatures(struct tree_ctx *ctx, const struct lysc_node *node, uint16_t mask, int level, int line_len)
{
    //struct ly_out *out = ctx->out;
    //uint8_t text_len = 0;
    //const char *text_str;

    //switch (node->nodetype & mask) {
    //case LYS_CONTAINER:
    //case LYS_LIST:
    //case LYS_CHOICE:
    //case LYS_CASE:
    //case LYS_ANYDATA:
    //case LYS_ANYXML:
    //case LYS_LEAF:
    //case LYS_LEAFLIST:
    //case LYS_RPC:
    //case LYS_ACTION:
    //case LYS_NOTIF:
    //case LYS_USES:
    //    if (node->parent && (node->parent->nodetype == LYS_AUGMENT)) {
    //        /* if-features from an augment are de facto inherited */
    //        //text_len = tree_print_features(out, node->iffeatures, node->iffeature_size,
    //        //                               node->parent->iffeature, node->parent->iffeature_size, opts, &text_str);
    //    } else {
    //        //text_len = tree_print_features(out, node->iffeature, node->iffeature_size, NULL, 0, opts, &text_str);
    //    }
    //    if (text_len) {
    //        line_len = tree_print_wrap(out, level, line_len, 1, text_len, ctx);
    //        line_len += ly_print(out, text_str);
    //        lydict_remove(ctx->module->ctx, text_str);
    //    }
    //    break;
    //default:
    //    /* only grouping */
    //    break;
    //}

    return line_len;
}

static void
trc_print_data_node(struct tree_ctx *ctx, const struct lysc_node *node, const struct lysc_node *aug_parent, uint16_t mask, int level, int subtree, uint16_t max_name_len)
{
    struct ly_out *out = ctx->out;
    int line_len = 0;
    int node_len = 0;

    /* disabled/not printed node */
    /* Skipped condition: (node->parent && node->parent->nodetype == LYS_AUGMENT) ? 1 : 0 */
    if(lysc_node_is_disabled(node, 0) || !(node->nodetype & mask)) {
        return;
    }

    /* implicit input/output/case */
    /* Skipped actions... */

    /* special uses and grouping handling */
    /* Skipped actions... */
    if(node->nodetype & LYS_ANYXML) {
        if (!node->parent && !strcmp(node->name, "config") && !strcmp(node->module->name, "ietf-netconf")) {
            /* node added by libyang, not actually in the model */
            return;
        }
    }

    /* print indent */
    line_len = gen_print_indent(out, ctx->base_indent, ctx->indent, level);
    /* print status */
    line_len += ly_print_(out, "%s--", (node->flags & LYS_STATUS_DEPRC ? "x" : (node->flags & LYS_STATUS_OBSLT ? "o" : "+")));
    ///* print config flags (or special opening for case, choice) */
    line_len += trc_print_config(out, ctx->spec_config, node->nodetype, node->flags);
    /* print optionally prefix */
    node_len = trc_print_prefix(out, node, ctx);
    /* print name */
    node_len += ly_print_(out, node->name);

    /* print one-character opts */
    node_len += trc_print_node_opts(out, node, mask);
    line_len += node_len;

    /* learn next level indent (there is never a sibling for subtree) */
    ++level;
    if (!subtree) {
        trc_next_indent(ctx, node, aug_parent, level);
    }

    /* print type/keys */
    line_len = trc_print_node_typekeys(ctx, node, mask, level, line_len, node_len, max_name_len);

    /* print default */
    line_len = trc_print_node_default(ctx, node, mask, level, line_len);

    /* print if-features */
    line_len = trc_print_node_iffeatures(ctx, node, mask, level, line_len);

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
trb_print_module_body(struct tree_ctx *ctx)
{
    struct lysc_node *node;

    ctx->base_indent = LY_TREE_MOD_DATA_INDENT;
    // mask = LYS_CHOICE | LYS_CONTAINER | LYS_LEAF | LYS_LEAFLIST | LYS_LIST | LYS_ANYDATA | LYS_USES;
    // max_child_len = tree_get_max_name_len(data, NULL, mask, &opts);

    /* LYS_RPC, LYS_NOTIF, LYS_GROUPING are skipped */
    LY_LIST_FOR(ctx->module->compiled->data, node) {
        /* TODO: we're printing the submodule only? */

        //ly_print_(ctx->out, "name: %s\n", node->name);

        uint16_t nodetype = node->nodetype;
        if(nodetype & (LYS_RPC | LYS_NOTIF))
            continue;
        else {
            uint16_t mask = LYS_CHOICE | LYS_CONTAINER | LYS_LEAF | LYS_LEAFLIST | LYS_LIST | LYS_ANYDATA;
            trc_print_data_node(ctx, node, NULL, mask, 0, 0, 0);
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

static void
trb_print_module_name(struct ly_out *out, const struct lys_module *module)
{
    /* TODO: printing 'submodule' keyword? */
    /* Skipped actions... */

    ly_print_(out, "module: %s\n", module->name);
}


LY_ERR tree_print_parsed_and_compiled_module(struct ly_out *out, const struct lys_module *module, uint32_t options)
{
    struct tree_ctx ctx_ = {.out = out, .module = module, .base_indent = 0, .line_length = 0, .spec_config = 0, .options = options}, *ctx = &ctx_;

    ly_print_(ctx->out, "------Sorry, tree_print_parsed_module is not fully implemented------\n");

    /* we are printing only a subtree */
    /* Skipped actions... */
    /* if (target_schema_path) { */

    trb_print_module_name(out, module);

    /* TODO: print subtree by target_schema_path */
    /* Skipped actions... */

    trb_print_module_body(ctx);

    ly_print_flush(out);
    return LY_SUCCESS;
}

//LY_ERR tree_print_parsed_submodule(struct ly_out *out, const struct lys_module *module, const struct lysp_submodule *submodp, uint32_t options)
LY_ERR tree_print_parsed_submodule(struct ly_out *out, const struct lys_module *UNUSED(module), const struct lysp_submodule *UNUSED(submodp), uint32_t UNUSED(options))
{
    ly_print_(out, "------Sorry, tree_print_parsed_submodule not implemented yet-----\n");
    return LY_SUCCESS;
}

//LY_ERR tree_print_compiled_node(struct ly_out *out, const struct lysc_node *node, uint32_t options)
LY_ERR tree_print_compiled_node(struct ly_out *out, const struct lysc_node *UNUSED(node), uint32_t UNUSED(options))
{
    ly_print_(out, "-----Sorry, tree_print_compiled_node not implemented yet-----\n");
    return LY_SUCCESS;
}

#if 0
static int
trc_sibling_is_valid_child(const struct lysc_node *node, int including, const struct lys_module *UNUSED(module),
                            const struct lysc_node *aug_parent, uint16_t nodetype)
{
    //struct lysc_node *cur, *cur2;
    struct lysc_node *cur;

    assert(!aug_parent || (aug_parent->nodetype == LYS_AUGMENT));

    if (!node) {
        return 0;
    } else if (!lysc_data_parent(node) && !strcmp(node->name, "config") && !strcmp(node->module->name, "ietf-netconf")) {
        /* node added by libyang, not actually in the model */
        return 0;
    }

    /* has a following printed child */
    const struct lysc_node *start = including ? node : node->next;
    LYSC_TREE_DFS_BEGIN(start, cur) {
        if (aug_parent && (cur->parent != aug_parent)) {
            /* we are done traversing this augment, the nodes are all direct siblings */
            return 0;
        }

        /* Skipped actions... */
        /* if (module->type && (lys_main_module(module) != lys_node_module(cur))) { */

        if (!lysc_node_is_disabled(cur, 0)) {
            /* Skipped actions... */
            /* if ((cur->nodetype == LYS_USES) || ((cur->nodetype == LYS_CASE) && (cur->flags & LYS_IMPLICIT))) { */
            switch (nodetype) {
            case LYS_GROUPING:
                /* we are printing groupings, they are printed separately */
                if (cur->nodetype == LYS_GROUPING) {
                    return 0;
                }
                break;
            case LYS_RPC:
                if (cur->nodetype == LYS_RPC) {
                    return 1;
                }
                break;
            case LYS_NOTIF:
                if (cur->nodetype == LYS_NOTIF) {
                    return 1;
                }
                break;
            default:
                if (cur->nodetype & (LYS_CONTAINER | LYS_LEAF | LYS_LEAFLIST | LYS_LIST | LYS_ANYDATA | LYS_CHOICE
                        | LYS_CASE | LYS_ACTION)) {
                    return 1;
                }

                /* Skipped condition */
                /* if ((cur->nodetype & (LYS_INPUT | LYS_OUTPUT)) && cur->child) { */

                /* only nested notifications count here (not top-level) */
                /* Skipped actions... */
                /* if (cur->nodetype == LYS_NOTIF) { */
                break;
            }
        }
        LYSC_TREE_DFS_END(start, cur)
    }

    /* if in uses, the following printed child can actually be in the parent node :-/ */
    /* Skipped actions... */
    /* if (lysc_data_parent(node) && (lysc_data_parent(node)->nodetype == LYS_USES)) { */

    return 0;
}
#endif
