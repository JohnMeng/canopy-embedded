/*
 * Copyright 2014 Gregory Prisament
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "canopy.h"
#include "canopy_internal.h"
#include "red_hash.h"
#include "red_json.h"
#include "red_log.h"
#include "red_string.h"
#include <unistd.h>
#include <assert.h>

bool _canopy_load_system_config(CanopyContext ctx)
{
    FILE * fp = canopy_open_config_file("canopy.conf.json");
    char *buffer;
    size_t filesize;
    char **keys;
    unsigned i;
    RedJsonObject confObj;
    if (!fp)
    {
        RedLog_Error("Could not locate canopy.conf.json");
        return false;
    }

    fseek(fp, 0, SEEK_END);
    filesize = ftell(fp); 
    fseek(fp, 0, SEEK_SET);
    buffer = calloc(1, filesize+1);
    fread(buffer, 1, filesize, fp);
    fclose(fp);

    confObj = RedJson_Parse(buffer);
    if (!confObj)
    {
        RedLog_Error("Error parsing canopy.conf.json");
        return false;
    }

    keys = RedJsonObject_NewKeysArray(confObj);
    for (i = 0; i < RedJsonObject_NumItems(confObj); i++)
    {
        if (!strcmp(keys[i], "cloud-host"))
        {
            if (!RedJsonObject_IsValueString(confObj, keys[i]))
            {
                RedLog_Warn("canopy.conf.json -- expected string for \"cloud-host\"");
                continue;
            }
            if (ctx->cloudHost)
                free(ctx->cloudHost);
            ctx->cloudHost = RedJsonObject_GetString(confObj, keys[i]);
        }
        else if (!strcmp(keys[i], "cloud-http-port"))
        {
            if (!RedJsonObject_IsValueNumber(confObj, keys[i]))
            {
                RedLog_Warn("canopy.conf.json -- expected number for \"cloud-http-port\"");
                continue;
            }
            ctx->cloudHttpPort = RedJsonObject_GetNumber(confObj, keys[i]);
        }
        else if (!strcmp(keys[i], "cloud-https-port"))
        {
            if (!RedJsonObject_IsValueNumber(confObj, keys[i]))
            {
                RedLog_Warn("canopy.conf.json -- expected number for \"cloud-https-port\"");
                continue;
            }
            ctx->cloudHttpsPort = RedJsonObject_GetNumber(confObj, keys[i]);
        }
        else if (!strcmp(keys[i], "cloud-use-https"))
        {
            if (!RedJsonObject_IsValueBoolean(confObj, keys[i]))
            {
                RedLog_Warn("canopy.conf.json -- expected boolean for \"cloud-use-https\"");
                continue;
            }
            ctx->cloudWebProtocol = RedJsonObject_GetBoolean(confObj, keys[i]) ? "https" : "http";
        }
        else
        {
            RedLog_Warn("canopy.conf.json -- unrecognized field: %s", keys[i]);
            continue;
        }
    }
    RedJsonObject_FreeKeysArray(keys);
    
    free(buffer);

    return true;
}
