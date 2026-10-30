/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2009      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2016      Research Organization for Information Science
 *                         and Technology (RIST). All rights reserved.
 * Copyright (c) 2016      Los Alamos National Security, LLC. All rights
 *                         reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 * SPDX-License-Identifier: BSD-3-Clause-Open-MPI
 */

#include "opal_config.h"

#include <stdlib.h>

#include "opal/constants.h"
#include "opal/mca/base/base.h"
#include "opal/mca/mca.h"
#include "opal/mca/memory/base/base.h"
#include "opal/mca/memory/base/empty.h"
#include "opal/mca/memory/memory.h"
#include "opal/util/argv.h"
#include "opal/util/output.h"

/*
 * The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct.
 */
#include "opal/mca/memory/base/static-components.h"

static int empty_process(void)
{
    return OPAL_SUCCESS;
}

static int empty_query(int *priority)
{
    *priority = 0;
    return OPAL_SUCCESS;
}

/*
 * Local variables
 */
static opal_memory_base_component_2_0_0_t empty_component = {
    /* Empty / safe functions to call if no memory component is selected */
    .memoryc_query = empty_query,
    .memoryc_process = empty_process,
    .memoryc_register = opal_memory_base_component_register_empty,
    .memoryc_deregister = opal_memory_base_component_deregister_empty,
    .memoryc_set_alignment = opal_memory_base_component_set_alignment_empty,
};

/*
 * Globals
 */
opal_memory_base_component_2_0_0_t *opal_memory = &empty_component;

void opal_memory_base_malloc_init_hook(void)
{
    if (opal_memory->memoryc_init_hook) {
        opal_memory->memoryc_init_hook();
    }
}

/*
 * Function for finding and opening either all MCA components, or the one
 * that was specifically requested via a MCA parameter.
 */
typedef struct {
    mca_base_component_list_item_t *item;
    int priority;
    bool attempted;
} memory_candidate_t;

static bool memory_selection_is_explicit(void)
{
    char **names = NULL;
    bool include_mode = false;

    if (OPAL_SUCCESS
        != mca_base_component_parse_requested(opal_memory_base_framework.framework_selection,
                                              &include_mode, &names)) {
        return false;
    }
    include_mode = include_mode && NULL != names;
    opal_argv_free(names);
    return include_mode;
}

static int opal_memory_base_open(mca_base_open_flag_t flags)
{
    opal_list_t *components = &opal_memory_base_framework.framework_components;
    int output = opal_memory_base_framework.framework_output;
    mca_base_component_list_item_t *item, *next, *winner = NULL;
    memory_candidate_t *candidates;
    size_t count = 0;
    int priority, ret;

    if (flags & MCA_BASE_OPEN_FIND_COMPONENTS) {
        ret = mca_base_component_find(NULL, &opal_memory_base_framework, false,
                                      !(flags & MCA_BASE_OPEN_STATIC_ONLY));
        if (OPAL_SUCCESS != ret) {
            return ret;
        }
    }

    ret = mca_base_components_filter(&opal_memory_base_framework, MCA_BASE_METADATA_PARAM_NONE);
    if (OPAL_SUCCESS != ret) {
        return ret;
    }

    candidates = calloc(opal_list_get_size(components) + 1, sizeof(*candidates));
    if (NULL == candidates) {
        return OPAL_ERR_OUT_OF_RESOURCE;
    }

    OPAL_LIST_FOREACH (item, components, mca_base_component_list_item_t) {
        opal_memory_base_component_2_0_0_t *tmp = (opal_memory_base_component_2_0_0_t *)
                                                      item->cli_component;
        size_t i;

        if (OPAL_SUCCESS != tmp->memoryc_query(&priority) || priority < 0) {
            continue;
        }

        for (i = count++; i > 0 && candidates[i - 1].priority <= priority; --i) {
            candidates[i] = candidates[i - 1];
        }
        candidates[i] = (memory_candidate_t){.item = item, .priority = priority};
    }

    for (size_t i = 0; i < count && NULL == winner; ++i) {
        const mca_base_component_t *component = candidates[i].item->cli_component;

        candidates[i].attempted = true;
        ret = NULL == component->mca_open_component ? OPAL_SUCCESS
                                                    : component->mca_open_component();
        if (OPAL_SUCCESS == ret) {
            winner = candidates[i].item;
        } else {
            if (OPAL_ERR_NOT_AVAILABLE != ret
                && mca_base_show_load_errors(component->mca_type_name,
                                             component->mca_component_name)) {
                opal_output_verbose(MCA_BASE_VERBOSE_ERROR, output,
                                    "mca: base: components_open: component %s "
                                    "/ %s open function failed",
                                    component->mca_type_name, component->mca_component_name);
            }
            opal_output_verbose(MCA_BASE_VERBOSE_COMPONENT, output,
                                "memory: component %s open failed (%d), trying the next one",
                                component->mca_component_name, ret);
        }
    }

    OPAL_LIST_FOREACH_SAFE (item, next, components, mca_base_component_list_item_t) {
        bool attempted = false;

        if (item == winner) {
            continue;
        }
        for (size_t i = 0; i < count; ++i) {
            if (candidates[i].item == item) {
                attempted = candidates[i].attempted;
                break;
            }
        }
        if (attempted) {
            mca_base_component_close(item->cli_component, output);
        } else {
            mca_base_component_unload(item->cli_component, output);
        }
        opal_list_remove_item(components, &item->super);
        OBJ_RELEASE(item);
    }
    free(candidates);

    if (NULL == winner) {
        opal_memory = &empty_component;
        if (memory_selection_is_explicit()) {
            ret = mca_base_components_filter(&opal_memory_base_framework,
                                             MCA_BASE_METADATA_PARAM_NONE);
            return OPAL_SUCCESS == ret ? OPAL_ERR_NOT_FOUND : ret;
        }
        return OPAL_SUCCESS;
    }

    opal_memory = (opal_memory_base_component_2_0_0_t *) winner->cli_component;
    opal_output_verbose(MCA_BASE_VERBOSE_COMPONENT, output, "memory: using component %s",
                        winner->cli_component->mca_component_name);
    return OPAL_SUCCESS;
}

/* Use default register/close functions */
MCA_BASE_FRAMEWORK_DECLARE(opal, memory, "memory hooks", NULL, opal_memory_base_open, NULL,
                           mca_memory_base_static_components, 0);
