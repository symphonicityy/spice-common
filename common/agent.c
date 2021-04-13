/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2020 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#include "agent.h"

#ifdef _WIN32
// Windows is always little endian
#  define FIX_ENDIAN16(x) (x) = (x)
#  define FIX_ENDIAN32(x) (x) = (x)
#  define FIX_ENDIAN64(x) (x) = (x)
#else
#  include <glib.h>
#  define FIX_ENDIAN16(x) (x) = GUINT16_FROM_LE(x)
#  define FIX_ENDIAN32(x) (x) = GUINT32_FROM_LE(x)
#  define FIX_ENDIAN64(x) (x) = GUINT64_FROM_LE(x)
#endif

#include <spice/start-packed.h>
typedef struct SPICE_ATTR_PACKED {
    uint16_t v;
} uint16_unaligned_t;

typedef struct SPICE_ATTR_PACKED {
    uint32_t v;
} uint32_unaligned_t;

typedef struct SPICE_ATTR_PACKED {
    uint64_t v;
} uint64_unaligned_t;
#include <spice/end-packed.h>

static const int agent_message_min_size[] =
{
    -1, /* Does not exist */
    sizeof(VDAgentMouseState), /* VD_AGENT_MOUSE_STATE */
    sizeof(VDAgentMonitorsConfig), /* VD_AGENT_MONITORS_CONFIG */
    sizeof(VDAgentReply), /* VD_AGENT_REPLY */
    sizeof(VDAgentClipboard), /* VD_AGENT_CLIPBOARD */
    sizeof(VDAgentDisplayConfig), /* VD_AGENT_DISPLAY_CONFIG */
    sizeof(VDAgentAnnounceCapabilities), /* VD_AGENT_ANNOUNCE_CAPABILITIES */
    sizeof(VDAgentClipboardGrab), /* VD_AGENT_CLIPBOARD_GRAB */
    sizeof(VDAgentClipboardRequest), /* VD_AGENT_CLIPBOARD_REQUEST */
    sizeof(VDAgentClipboardRelease), /* VD_AGENT_CLIPBOARD_RELEASE */
    sizeof(VDAgentFileXferStartMessage), /* VD_AGENT_FILE_XFER_START */
    sizeof(VDAgentFileXferStatusMessage), /* VD_AGENT_FILE_XFER_STATUS */
    sizeof(VDAgentFileXferDataMessage), /* VD_AGENT_FILE_XFER_DATA */
    0, /* VD_AGENT_CLIENT_DISCONNECTED */
    sizeof(VDAgentMaxClipboard), /* VD_AGENT_MAX_CLIPBOARD */
    sizeof(VDAgentAudioVolumeSync), /* VD_AGENT_AUDIO_VOLUME_SYNC */
    sizeof(VDAgentGraphicsDeviceInfo), /* VD_AGENT_GRAPHICS_DEVICE_INFO */
};

static AgentCheckResult
agent_message_check_size(const VDAgentMessage *message_header,
                         const uint32_t *capabilities, uint32_t capabilities_size)
{
    if (message_header->protocol != VD_AGENT_PROTOCOL) {
        return AGENT_CHECK_WRONG_PROTOCOL_VERSION;
    }

    if (message_header->type >= SPICE_N_ELEMENTS(agent_message_min_size) ||
        agent_message_min_size[message_header->type] < 0) {
        return AGENT_CHECK_UNKNOWN_MESSAGE;
    }

    uint32_t min_size = agent_message_min_size[message_header->type];

    if (VD_AGENT_HAS_CAPABILITY(capabilities, capabilities_size,
                                VD_AGENT_CAP_CLIPBOARD_SELECTION)) {
        switch (message_header->type) {
        case VD_AGENT_CLIPBOARD_GRAB:
        case VD_AGENT_CLIPBOARD_REQUEST:
        case VD_AGENT_CLIPBOARD:
        case VD_AGENT_CLIPBOARD_RELEASE:
            min_size += 4;
        }
    }

    if (VD_AGENT_HAS_CAPABILITY(capabilities, capabilities_size,
                                VD_AGENT_CAP_CLIPBOARD_GRAB_SERIAL)
        && message_header->type == VD_AGENT_CLIPBOARD_GRAB) {
        min_size += 4;
    }

    switch (message_header->type) {
    case VD_AGENT_MONITORS_CONFIG:
    case VD_AGENT_FILE_XFER_START:
    case VD_AGENT_FILE_XFER_DATA:
    case VD_AGENT_CLIPBOARD:
    case VD_AGENT_CLIPBOARD_GRAB:
    case VD_AGENT_AUDIO_VOLUME_SYNC:
    case VD_AGENT_ANNOUNCE_CAPABILITIES:
    case VD_AGENT_GRAPHICS_DEVICE_INFO:
    case VD_AGENT_FILE_XFER_STATUS:
        if (message_header->size < min_size) {
            return AGENT_CHECK_INVALID_SIZE;
        }
        break;
    case VD_AGENT_MOUSE_STATE:
    case VD_AGENT_DISPLAY_CONFIG:
    case VD_AGENT_REPLY:
    case VD_AGENT_CLIPBOARD_REQUEST:
    case VD_AGENT_CLIPBOARD_RELEASE:
    case VD_AGENT_MAX_CLIPBOARD:
    case VD_AGENT_CLIENT_DISCONNECTED:
        if (message_header->size != min_size) {
            return AGENT_CHECK_INVALID_SIZE;
        }
        break;
    default:
        return AGENT_CHECK_UNKNOWN_MESSAGE;
    }
    return AGENT_CHECK_NO_ERROR;
}

static void uint16_from_le(uint8_t *_msg, uint32_t size, uint32_t offset)
{
    uint32_t i;
    uint16_unaligned_t *msg = (uint16_unaligned_t *)(_msg + offset);

    /* size % 2 should be 0 - extra bytes are ignored */
    for (i = 0; i < size / 2; i++) {
        FIX_ENDIAN16(msg[i].v);
    }
}

static void uint32_from_le(uint8_t *_msg, uint32_t size, uint32_t offset)
{
    uint32_t i;
    uint32_unaligned_t *msg = (uint32_unaligned_t *)(_msg + offset);

    /* size % 4 should be 0 - extra bytes are ignored */
    for (i = 0; i < size / 4; i++) {
        FIX_ENDIAN32(msg[i].v);
    }
}

static void
agent_message_clipboard_from_le(const VDAgentMessage *message_header, uint8_t *data,
                                const uint32_t *capabilities, uint32_t capabilities_size)
{
    size_t min_size = agent_message_min_size[message_header->type];
    uint32_unaligned_t *data_type = (uint32_unaligned_t *) data;

    if (VD_AGENT_HAS_CAPABILITY(capabilities, capabilities_size,
                                VD_AGENT_CAP_CLIPBOARD_SELECTION)) {
        min_size += 4;
        data_type++;
    }

    switch (message_header->type) {
    case VD_AGENT_CLIPBOARD_REQUEST:
    case VD_AGENT_CLIPBOARD:
        FIX_ENDIAN32(data_type->v);
        break;
    case VD_AGENT_CLIPBOARD_GRAB:
        uint32_from_le(data, message_header->size - min_size, min_size);
        break;
    case VD_AGENT_CLIPBOARD_RELEASE:
        // empty
        break;
    }
}

static void
agent_message_file_xfer_from_le(const VDAgentMessage *message_header, uint8_t *data)
{
    uint32_unaligned_t *id = (uint32_unaligned_t *)data;
    FIX_ENDIAN32(id->v);
    id++; // result

    switch (message_header->type) {
    case VD_AGENT_FILE_XFER_DATA: {
        VDAgentFileXferDataMessage *msg = (VDAgentFileXferDataMessage *) data;
        FIX_ENDIAN64(msg->size);
        break;
    }
    case VD_AGENT_FILE_XFER_STATUS: {
        VDAgentFileXferStatusMessage *msg = (VDAgentFileXferStatusMessage *) data;
        FIX_ENDIAN32(msg->result);
        // from client/server we don't expect any detail
        switch (msg->result) {
        case VD_AGENT_FILE_XFER_STATUS_NOT_ENOUGH_SPACE:
            if (message_header->size >= sizeof(VDAgentFileXferStatusMessage) +
                sizeof(VDAgentFileXferStatusNotEnoughSpace)) {
                VDAgentFileXferStatusNotEnoughSpace *err =
                    (VDAgentFileXferStatusNotEnoughSpace*) msg->data;
                FIX_ENDIAN64(err->disk_free_space);
            }
            break;
        case VD_AGENT_FILE_XFER_STATUS_ERROR:
            if (message_header->size >= sizeof(VDAgentFileXferStatusMessage) +
                sizeof(VDAgentFileXferStatusError)) {
                VDAgentFileXferStatusError *err =
                    (VDAgentFileXferStatusError *) msg->data;
                FIX_ENDIAN32(err->error_code);
            }
            break;
        }
        break;
    }
    }
}

static AgentCheckResult
agent_message_graphics_device_info_check_from_le(const VDAgentMessage *message_header,
                                                 uint8_t *data)
{
    uint8_t *const end = data + message_header->size;
    uint32_unaligned_t *u32 = (uint32_unaligned_t *) data;
    FIX_ENDIAN32(u32->v);
    const uint32_t count = u32->v;
    data += 4;

    for (size_t i = 0; i < count; ++i) {
        if ((size_t) (end - data) < sizeof(VDAgentDeviceDisplayInfo)) {
            return AGENT_CHECK_TRUNCATED;
        }
        uint32_from_le(data, sizeof(VDAgentDeviceDisplayInfo), 0);
        VDAgentDeviceDisplayInfo *info = (VDAgentDeviceDisplayInfo *) data;
        data += sizeof(VDAgentDeviceDisplayInfo);
        if (!info->device_address_len) {
            return AGENT_CHECK_INVALID_DATA;
        }
        if ((size_t) (end - data) < info->device_address_len) {
            return AGENT_CHECK_TRUNCATED;
        }
        info->device_address[info->device_address_len - 1] = 0;
        data += info->device_address_len;
    }
    return AGENT_CHECK_NO_ERROR;
}

static AgentCheckResult
agent_message_monitors_config_from_le(const VDAgentMessage *message_header, uint8_t *message)
{
    uint32_from_le(message, sizeof(VDAgentMonitorsConfig), 0);
    VDAgentMonitorsConfig *vdata = (VDAgentMonitorsConfig*) message;
    vdata->flags &= VD_AGENT_CONFIG_MONITORS_FLAG_USE_POS|
        VD_AGENT_CONFIG_MONITORS_FLAG_PHYSICAL_SIZE;
    size_t element_size = sizeof(vdata->monitors[0]);
    if ((vdata->flags & VD_AGENT_CONFIG_MONITORS_FLAG_PHYSICAL_SIZE) != 0) {
        element_size += sizeof(VDAgentMonitorMM);
    }
    const size_t max_monitors =
        (message_header->size - sizeof(*vdata)) / element_size;
    if (vdata->num_of_monitors > max_monitors) {
        return AGENT_CHECK_TRUNCATED;
    }
    uint32_from_le(message, sizeof(vdata->monitors[0]) * vdata->num_of_monitors,
                   sizeof(*vdata));
    if ((vdata->flags & VD_AGENT_CONFIG_MONITORS_FLAG_PHYSICAL_SIZE) != 0) {
        uint16_from_le(message, sizeof(VDAgentMonitorMM) * vdata->num_of_monitors,
                       sizeof(*vdata) + sizeof(vdata->monitors[0]) * vdata->num_of_monitors);
    }
    return AGENT_CHECK_NO_ERROR;
}

AgentCheckResult
agent_check_message(const VDAgentMessage *message_header, uint8_t *message,
                    const uint32_t *capabilities, uint32_t capabilities_size)
{
    AgentCheckResult res;

    res = agent_message_check_size(message_header, capabilities, capabilities_size);
    if (res != AGENT_CHECK_NO_ERROR) {
        return res;
    }

    switch (message_header->type) {
    case VD_AGENT_MOUSE_STATE:
        uint32_from_le(message, 3 * sizeof(uint32_t), 0);
        break;
    case VD_AGENT_REPLY:
    case VD_AGENT_DISPLAY_CONFIG:
    case VD_AGENT_MAX_CLIPBOARD:
    case VD_AGENT_ANNOUNCE_CAPABILITIES:
        uint32_from_le(message, message_header->size, 0);
        break;
    case VD_AGENT_MONITORS_CONFIG:
        return agent_message_monitors_config_from_le(message_header, message);

    case VD_AGENT_CLIPBOARD:
    case VD_AGENT_CLIPBOARD_GRAB:
    case VD_AGENT_CLIPBOARD_REQUEST:
    case VD_AGENT_CLIPBOARD_RELEASE:
        agent_message_clipboard_from_le(message_header, message,
                                        capabilities, capabilities_size);
        break;
    case VD_AGENT_FILE_XFER_START:
    case VD_AGENT_FILE_XFER_STATUS:
    case VD_AGENT_FILE_XFER_DATA:
        agent_message_file_xfer_from_le(message_header, message);
        break;
    case VD_AGENT_CLIENT_DISCONNECTED:
        break;
    case VD_AGENT_GRAPHICS_DEVICE_INFO:
        return agent_message_graphics_device_info_check_from_le(message_header, message);

    case VD_AGENT_AUDIO_VOLUME_SYNC: {
        VDAgentAudioVolumeSync *vdata = (VDAgentAudioVolumeSync *)message;
        const size_t max_channels =
            (message_header->size - sizeof(*vdata)) / sizeof(vdata->volume[0]);
        if (vdata->nchannels > max_channels) {
            return AGENT_CHECK_TRUNCATED;
        }
        uint16_from_le(message, message_header->size - sizeof(*vdata), sizeof(*vdata));
        break;
    }
    default:
        return AGENT_CHECK_UNKNOWN_MESSAGE;
    }
    return AGENT_CHECK_NO_ERROR;
}

void
agent_prepare_filexfer_status(AgentFileXferStatusMessageFull *status, size_t *status_size,
                              const uint32_t *capabilities, uint32_t capabilities_size)
{
    if (*status_size < sizeof(status->common)) {
        *status_size = sizeof(status->common);
    }

    // if there are details but no cap for detail remove it
    if (!VD_AGENT_HAS_CAPABILITY(capabilities, capabilities_size,
                                 VD_AGENT_CAP_FILE_XFER_DETAILED_ERRORS)) {
        *status_size = sizeof(status->common);

        // if detail cap is not provided and error > threshold set to error
        if (status->common.result >= VD_AGENT_FILE_XFER_STATUS_NOT_ENOUGH_SPACE) {
            status->common.result = VD_AGENT_FILE_XFER_STATUS_ERROR;
        }
    }

    // fix endian
    switch (status->common.result) {
    case VD_AGENT_FILE_XFER_STATUS_NOT_ENOUGH_SPACE:
        FIX_ENDIAN64(status->not_enough_space.disk_free_space);
        break;
    case VD_AGENT_FILE_XFER_STATUS_ERROR:
        FIX_ENDIAN32(status->error.error_code);
        break;
    }
    // header should be done last
    FIX_ENDIAN32(status->common.id);
    FIX_ENDIAN32(status->common.result);
}
