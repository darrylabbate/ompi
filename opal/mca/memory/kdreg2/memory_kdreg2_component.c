/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2026      Amazon.com, Inc. or its affiliates.
 *                         All Rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 * SPDX-License-Identifier: BSD-3-Clause-Open-MPI
 */

#include "opal_config.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "memory_kdreg2.h"

#include "opal/constants.h"
#include "opal/mca/memory/base/base.h"
#include "opal/mca/memory/base/empty.h"
#include "opal/memoryhooks/memory_internal.h"
#include "opal/util/output.h"

static int kdreg2_open(void);
static int kdreg2_close(void);
static int kdreg2_register(void);
static int kdreg2_query(int *priority);

static int mca_memory_kdreg2_priority;
static int mca_memory_kdreg2_max_regions;

opal_memory_kdreg2_component_t mca_memory_kdreg2_component = {
    .super =
        {
            .memoryc_version =
                {
                    OPAL_MEMORY_BASE_VERSION_2_0_0,

                    .mca_component_name = "kdreg2",
                    MCA_BASE_MAKE_VERSION(component, OPAL_MAJOR_VERSION, OPAL_MINOR_VERSION,
                                          OPAL_RELEASE_VERSION),

                    .mca_open_component = kdreg2_open,
                    .mca_close_component = kdreg2_close,
                    .mca_register_component_params = kdreg2_register,
                },
            .memoryc_data = {MCA_BASE_METADATA_PARAM_CHECKPOINT},

            .memoryc_query = kdreg2_query,
            .memoryc_register = opal_memory_base_component_register_empty,
            .memoryc_deregister = opal_memory_base_component_deregister_empty,
            .memoryc_set_alignment = opal_memory_base_component_set_alignment_empty,
        },
    .fd = -1,
    .exit_pipe = {-1, -1},
};
MCA_BASE_COMPONENT_INIT(opal, memory, kdreg2)

static int kdreg2_register(void)
{
    mca_memory_kdreg2_priority = 10;
    mca_base_component_var_register(&mca_memory_kdreg2_component.super.memoryc_version, "priority",
                                    "Priority of the kdreg2 memory hook component",
                                    MCA_BASE_VAR_TYPE_INT, NULL, 0, 0, OPAL_INFO_LVL_5,
                                    MCA_BASE_VAR_SCOPE_CONSTANT, &mca_memory_kdreg2_priority);

    mca_memory_kdreg2_max_regions = 65536;
    mca_base_component_var_register(&mca_memory_kdreg2_component.super.memoryc_version,
                                    "max_regions",
                                    "Maximum number of regions kdreg2 monitors at once",
                                    MCA_BASE_VAR_TYPE_INT, NULL, 0, 0, OPAL_INFO_LVL_5,
                                    MCA_BASE_VAR_SCOPE_CONSTANT, &mca_memory_kdreg2_max_regions);

    return OPAL_SUCCESS;
}

static int kdreg2_device_open(int *fd, const struct kdreg2_status_data **status_data)
{
    struct kdreg2_config_data config_data = {
        .max_regions = (size_t) mca_memory_kdreg2_max_regions,
    };

    if (mca_memory_kdreg2_max_regions <= 0) {
        return OPAL_ERR_BAD_PARAM;
    }

    *fd = open(KDREG2_DEVICE_NAME, O_RDWR | O_CLOEXEC | O_NONBLOCK);
    if (*fd < 0) {
        opal_output_verbose(10, opal_memory_base_framework.framework_output,
                            "memory:kdreg2: cannot open %s: %s", KDREG2_DEVICE_NAME,
                            strerror(errno));
        return OPAL_ERR_NOT_AVAILABLE;
    }

    if (0 != ioctl(*fd, KDREG2_IOCTL_CONFIG_DATA, &config_data)) {
        opal_output_verbose(10, opal_memory_base_framework.framework_output,
                            "memory:kdreg2: KDREG2_IOCTL_CONFIG_DATA failed: %s",
                            strerror(errno));
        close(*fd);
        *fd = -1;
        return OPAL_ERR_NOT_AVAILABLE;
    }

    if (NULL != status_data) {
        *status_data = config_data.status_data;
    }

    return OPAL_SUCCESS;
}

static int kdreg2_query(int *priority)
{
    int fd;

    if (OPAL_SUCCESS != kdreg2_device_open(&fd, NULL)) {
        *priority = -1;
        return OPAL_SUCCESS;
    }
    close(fd);

    *priority = mca_memory_kdreg2_priority;
    return OPAL_SUCCESS;
}

static int kdreg2_read_events(int fd)
{
    struct kdreg2_event event;
    ssize_t bytes;

    while (1) {
        bytes = read(fd, &event, sizeof(event));
        if (bytes < 0) {
            if (EINTR == errno) {
                continue;
            }
            if (EAGAIN == errno || EWOULDBLOCK == errno) {
                return OPAL_SUCCESS;
            }
            return OPAL_ERROR;
        }
        if (sizeof(event) != (size_t) bytes) {
            return OPAL_ERROR;
        }
        if (KDREG2_EVENT_MAPPING_CHANGE == event.type) {
            opal_mem_hooks_release_hook((void *) event.u.mapping_change.addr,
                                        event.u.mapping_change.len, false);
        }
    }
}

static void *kdreg2_evictor(void *arg)
{
    struct pollfd fds[2] = {
        {.fd = mca_memory_kdreg2_component.fd, .events = POLLIN},
        {.fd = mca_memory_kdreg2_component.exit_pipe[0], .events = POLLIN},
    };

    while (1) {
        if (poll(fds, 2, -1) < 0) {
            if (EINTR == errno) {
                continue;
            }
            break;
        }
        if (fds[1].revents) {
            break;
        }
        if (fds[0].revents && OPAL_SUCCESS != kdreg2_read_events(fds[0].fd)) {
            break;
        }
    }

    return NULL;
}

static void kdreg2_cleanup(void)
{
    opal_memory_kdreg2_component_t *c = &mca_memory_kdreg2_component;

    if (c->fd >= 0) {
        close(c->fd);
        c->fd = -1;
        c->status_data = NULL;
    }
    for (int i = 0; i < 2; ++i) {
        if (c->exit_pipe[i] >= 0) {
            close(c->exit_pipe[i]);
            c->exit_pipe[i] = -1;
        }
    }
}

static int kdreg2_open(void)
{
    opal_memory_kdreg2_component_t *c = &mca_memory_kdreg2_component;
    int rc;

    if (c->fd >= 0) {
        return OPAL_SUCCESS;
    }

    rc = kdreg2_device_open(&c->fd, &c->status_data);
    if (OPAL_SUCCESS != rc) {
        return rc;
    }

    if (0 != pipe2(c->exit_pipe, O_CLOEXEC)) {
        kdreg2_cleanup();
        return OPAL_ERR_OUT_OF_RESOURCE;
    }

    if (0 != pthread_create(&c->thread, NULL, kdreg2_evictor, NULL)) {
        kdreg2_cleanup();
        return OPAL_ERR_OUT_OF_RESOURCE;
    }

    opal_output_verbose(10, opal_memory_base_framework.framework_output,
                        "memory:kdreg2: monitor started");
    return OPAL_SUCCESS;
}

static int kdreg2_close(void)
{
    opal_memory_kdreg2_component_t *c = &mca_memory_kdreg2_component;

    if (c->fd < 0) {
        return OPAL_SUCCESS;
    }

    if (1 == write(c->exit_pipe[1], "x", 1)) {
        pthread_join(c->thread, NULL);
    } else {
        pthread_cancel(c->thread);
        pthread_join(c->thread, NULL);
    }

    kdreg2_cleanup();
    return OPAL_SUCCESS;
}
