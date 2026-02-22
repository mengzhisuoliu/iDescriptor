/*
 * iDescriptor: A free and open-source idevice management tool.
 *
 * Copyright (C) 2025 Uncore <https://github.com/uncor3>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

// https://github.com/libimobiledevice/libimobiledevice/blob/master/tools/ideviceinfo.c
/*
 * ideviceinfo.c
 * Simple utility to show information about an attached device
 *
 * Copyright (c) 2010-2019 Nikias Bassen, All Rights Reserved.
 * Copyright (c) 2009 Martin Szulecki All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <signal.h>
#endif

#include "../../iDescriptor.h"
#include <QDebug>
#include <plist/plist.h>
#include <pugixml.hpp>

#define FORMAT_KEY_VALUE 1
#define FORMAT_XML 2

static const char *domains[] = {
    "com.apple.disk_usage", "com.apple.disk_usage.factory",
    "com.apple.mobile.battery",
    /* FIXME: For some reason lockdownd segfaults on this, works sometimes
       though "com.apple.mobile.debug",. */
    "com.apple.iqagent", "com.apple.purplebuddy", "com.apple.PurpleBuddy",
    "com.apple.mobile.chaperone", "com.apple.mobile.third_party_termination",
    "com.apple.mobile.lockdownd", "com.apple.mobile.lockdown_cache",
    "com.apple.xcode.developerdomain", "com.apple.international",
    "com.apple.mobile.data_sync", "com.apple.mobile.tethered_sync",
    "com.apple.mobile.mobile_application_usage", "com.apple.mobile.backup",
    "com.apple.mobile.nikita", "com.apple.mobile.restriction",
    "com.apple.mobile.user_preferences", "com.apple.mobile.sync_data_class",
    "com.apple.mobile.software_behavior",
    "com.apple.mobile.iTunes.SQLMusicLibraryPostProcessCommands",
    "com.apple.mobile.iTunes.accessories",
    "com.apple.mobile.internal",          /**< iOS 4.0+ */
    "com.apple.mobile.wireless_lockdown", /**< iOS 4.0+ */
    "com.apple.fairplay", "com.apple.iTunes", "com.apple.mobile.iTunes.store",
    "com.apple.mobile.iTunes", "com.apple.fmip", "com.apple.Accessibility",
    NULL};

plist_t get_device_info(const char *udid, LockdowndClientHandle *client)
{
    IdeviceFfiError *err;
    plist_t node = NULL;

    /* run query and output information */
    err = lockdownd_get_value(client, NULL, NULL, &node);

    if (err) {
        qDebug() << "ERROR: Could not get value";
    }

    plist_t disk_info = nullptr;
    uint64_t total_space = 0;
    uint64_t free_space = 0;
    err = lockdownd_get_value(client, nullptr, "com.apple.disk_usage",
                              &disk_info);
    qDebug() << "Disk usage fetch error:" << (err ? err->message : "none");

    if (!err) {
        qDebug() << "Disk Usage Info:";
        plist_print(disk_info);

        // merge dict
        plist_dict_merge(&node, disk_info);
        // plist_free(disk_info);
    }

    return node;
}

void get_device_info_xml(const char *udid, LockdowndClientHandle *client,
                         pugi::xml_document &infoXml)
{
    plist_t node = get_device_info(udid, client);
    if (!node)
        return;
    char *xml_string = nullptr;
    uint32_t xml_length = 0;
    plist_to_xml(node, &xml_string, &xml_length);
    plist_free(node);

    if (xml_string) {
        infoXml.load_string(xml_string);
        // FIXME: crashes on Windows
        // free(xml_string);
    }
}