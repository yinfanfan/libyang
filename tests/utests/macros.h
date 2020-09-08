/**
 * @file   macros.h 
 * @author Radek Iša <isa@cesnet.cz>
 * @brief  this file contains macros for simplification test writing 
 *
 * Copyright (c) 2020 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#ifndef _LY_UTEST_MACROS_H_
#define _LY_UTEST_MACROS_H_

// cmocka header files
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

//local header files
#include "libyang.h"
#include "tests/config.h"

//macros
#define CONTEXT_GET \
                        ly_context

#define CONTEXT_CREATE_PATH(PATH) \
                        struct ly_ctx *ly_context;\
                        assert_int_equal(LY_SUCCESS, ly_ctx_new(PATH, 0, &ly_context));\

#define CONTEXT_DESTROY \
                        ly_ctx_destroy(ly_context, NULL);


#define MODEL_CREATE_PARAM(INPUT, INPUT_FORMAT, PARAM_PARSE, PARAM_VALID, OUT_MODEL) \
                        struct lyd_node * OUT_MODEL;\
                        assert_int_equal(LY_SUCCESS, lyd_parse_data_mem(ly_context, INPUT, INPUT_FORMAT, PARAM_PARSE, PARAM_VALID, & OUT_MODEL));\
                        assert_non_null(OUT_MODEL);

#define MODEL_DESTROY(MODEL) \
                        lyd_free_siblings(MODEL); 

#define MODEL_CHECK_CHAR_PARAM(IN_MODEL, TEXT, PARAM) \
                         {\
                             char * test;\
                             lyd_print_mem(&test, IN_MODEL, LYD_XML, PARAM);\
                             assert_string_equal(test, TEXT);\
                             free(test);\
                         }


#define MODEL_CHECK(IN_MODEL_1, IN_MODEL_2) \
                         {\
                             char * test_1;\
                             char * test_2;\
                             lyd_print_mem(&test_1, IN_MODEL_1, LYD_XML, LYD_PRINT_WITHSIBLINGS | LYD_PRINT_SHRINK);\
                             lyd_print_mem(&test_2, IN_MODEL_2, LYD_XML, LYD_PRINT_WITHSIBLINGS | LYD_PRINT_SHRINK);\
                             assert_string_equal(test_1, test_2);\
                             free(test_1);\
                             free(test_2);\
                         }


#endif
