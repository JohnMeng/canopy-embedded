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

CanopyContext canopy_init()
{
    CanopyContext ctx = NULL;
    bool result;
    char *uuid = NULL;
    ctx = calloc(1, sizeof(CanopyContext_t));
    if (!ctx)
    {
        /* TODO: set error */
        goto fail;
    }

    ctx->properties = RedHash_New(0);
    if (!ctx->properties)
    {
        goto fail;
    }
    ctx->initialized = true;

    /* Set defaults */
    ctx->cloudHost = RedString_strdup("canopy.link");
    ctx->cloudHttpPort = 80;
    ctx->cloudHttpsPort = 433;
    ctx->cloudWebProtocol = "https";
    _canopy_load_system_config(ctx);

    uuid = canopy_read_system_uuid();
    if (!uuid)
    {
        RedLog_Warn("Could not determine device UUID.");
        RedLog_Warn("Please run 'sudo cano uuid --install'.");
        RedLog_Warn("Or call canopy_set_device_id before canopy_connect");
    }
    else
    {
        result = canopy_set_device_id(ctx, uuid);
        if (!result)
        {
            RedLog_Error("Could not set UUID to %s", uuid);
            goto fail;
        }
    }

    free(uuid);
    return ctx;
fail:
    free(uuid);
    if (ctx)
    {
        /*RedHash_Free(ctx->properties);*/
        free(ctx);
    }
    return NULL;
}

bool canopy_set_device_id(CanopyContext canopy, const char *uuid)
{
    canopy->uuid = calloc(1, strlen(uuid)+1);
    if (!canopy->uuid)
    {
        return false;
    }
    strcpy(canopy->uuid, uuid);
    return true;
}

bool canopy_set_device_id_filename(CanopyContext canopy, const char *filename)
{
    FILE *fp;
    bool result;
    long filesize;
    char *buffer;

    fp = fopen(filename, "r");
    if (!fp)
    {
        return false;
    }

    fseek(fp, 0, SEEK_END);
    filesize = ftell(fp); 
    fseek(fp, 0, SEEK_SET);
    buffer = calloc(1, filesize+1);
    fread(buffer, 1, filesize, fp);
    result = canopy_set_device_id(canopy, buffer);
    free(buffer);
    fclose(fp);

    return result;
}

bool canopy_register_event_callback(CanopyContext canopy, CanopyEventCallbackRoutine fn, void *extra)
{
    canopy->cb = fn;
    canopy->cbExtra = extra;
    return true;
}


CanopyReport canopy_begin_report(CanopyContext canopy)
{
    CanopyReport report = NULL;
    report = calloc(1, sizeof(CanopyReport_t));
    if (!report)
    {
        goto fail;
    }

    report->values = RedHash_New(0);
    if (!report->values)
    {
        goto fail;
    }

    report->ctx = canopy; /* TODO: refcnt? */

    return report;
fail:
    if (report)
    {
        /*RedHash_Free(report->values);*/
        free(report);
    }
    return NULL;
}

bool _canopy_report_generic(CanopyReport report, const char *parameter, const _CanopyPropertyValue *propval)
{
    CanopyContext ctx = report->ctx;
    _CanopyPropertyValue *propvalCopy;
    _CanopyPropertyValue *oldValue;

    SDDLSensor sensor = sddl_class_lookup_sensor(ctx->sddl, parameter);
    if (!sensor)
    {
        RedLog_Warn("Device does not have sensor %s", parameter);
        return false;
    }

    if (report->finished)
    {
        /* canopy_end_report already called, cannot make further changes. */
        RedLog_Warn("Cannot report values after canopy_end_report has been called", 0);
        return false;
    }

    if (sddl_sensor_datatype(sensor) != propval->datatype)
    {
        /* incorrect datatype */
        RedLog_Warn("Incorrect datatype reported for %s", parameter);
        return false;
    }

    /* validate input value */
    switch (propval->datatype)
    {
        case SDDL_DATATYPE_INT8:
        {
            int8_t value = propval->val.val_int8;
            if (sddl_sensor_min_value(sensor) && value < (int8_t)*sddl_sensor_min_value(sensor))
            {
                /* value too low */
                return false;
            }
            else if (sddl_sensor_max_value(sensor) && value > (int8_t)*sddl_sensor_max_value(sensor))
            {
                /* value too large */
                return false;
            }
            break;
        }
        case SDDL_DATATYPE_UINT8:
        {
            uint8_t value = propval->val.val_uint8;
            if (sddl_sensor_min_value(sensor) && value < (uint8_t)*sddl_sensor_min_value(sensor))
            {
                /* value too low */
                return false;
            }
            else if (sddl_sensor_max_value(sensor) && value > (uint8_t)*sddl_sensor_max_value(sensor))
            {
                /* value too large */
                return false;
            }
            break;
        }
        case SDDL_DATATYPE_INT16:
        {
            int16_t value = propval->val.val_int16;
            if (sddl_sensor_min_value(sensor) && value < (int16_t)*sddl_sensor_min_value(sensor))
            {
                /* value too low */
                return false;
            }
            else if (sddl_sensor_max_value(sensor) && value > (int16_t)*sddl_sensor_max_value(sensor))
            {
                /* value too large */
                return false;
            }
            break;
        }
        case SDDL_DATATYPE_UINT16:
        {
            int16_t value = propval->val.val_uint16;
            if (sddl_sensor_min_value(sensor) && value < (uint16_t)*sddl_sensor_min_value(sensor))
            {
                /* value too low */
                return false;
            }
            else if (sddl_sensor_max_value(sensor) && value > (uint16_t)*sddl_sensor_max_value(sensor))
            {
                /* value too large */
                return false;
            }
            break;
        }
        case SDDL_DATATYPE_INT32:
        {
            int16_t value = propval->val.val_int32;
            if (sddl_sensor_min_value(sensor) && value < (int32_t)*sddl_sensor_min_value(sensor))
            {
                /* value too low */
                return false;
            }
            else if (sddl_sensor_max_value(sensor) && value > (int32_t)*sddl_sensor_max_value(sensor))
            {
                /* value too large */
                return false;
            }
            break;
        }
        case SDDL_DATATYPE_UINT32:
        {
            int16_t value = propval->val.val_uint32;
            if (sddl_sensor_min_value(sensor) && value < (uint32_t)*sddl_sensor_min_value(sensor))
            {
                /* value too low */
                return false;
            }
            else if (sddl_sensor_max_value(sensor) && value > (uint32_t)*sddl_sensor_max_value(sensor))
            {
                /* value too large */
                return false;
            }
            break;
        }
        case SDDL_DATATYPE_FLOAT32:
        {
            int16_t value = propval->val.val_float32;
            if (sddl_sensor_min_value(sensor) && value < (float)*sddl_sensor_min_value(sensor))
            {
                /* value too low */
                return false;
            }
            else if (sddl_sensor_max_value(sensor) && value > (float)*sddl_sensor_max_value(sensor))
            {
                /* value too large */
                return false;
            }
            break;
        }
        case SDDL_DATATYPE_FLOAT64:
        {
            int16_t value = propval->val.val_float64;
            if (sddl_sensor_min_value(sensor) && value < *sddl_sensor_min_value(sensor))
            {
                /* value too low */
                return false;
            }
            else if (sddl_sensor_max_value(sensor) && value > *sddl_sensor_max_value(sensor))
            {
                /* value too large */
                return false;
            }
            break;
        }
        case SDDL_DATATYPE_STRING:
        {
            /* TODO: Regex checking */
            break;
        }
        default:
        {
            break;
        }
    }

    /* copy property value object */
    propvalCopy = malloc(sizeof(_CanopyPropertyValue));
    if (!propval)
    {
        /* allocation failed */
        return false;
    }
    memcpy(propvalCopy, propval, sizeof(_CanopyPropertyValue));

    /* Add it to report's hash table */
    if (RedHash_UpdateOrInsertS(report->values, (void **)&oldValue, parameter, propvalCopy))
    {
        /* Free any value that it replaced */
        free(oldValue);
    }
    return true;
}

bool canopy_report_void(CanopyReport report, const char *parameter)
{
    _CanopyPropertyValue propval;
    propval.datatype = SDDL_DATATYPE_VOID;
    return _canopy_report_generic(report, parameter, &propval);
}
bool canopy_report_string(CanopyReport report, const char *parameter, const char *value)
{
    _CanopyPropertyValue propval;
    propval.datatype = SDDL_DATATYPE_STRING;
    propval.val.val_string = RedString_strdup(value);
    return _canopy_report_generic(report, parameter, &propval);
}
bool canopy_report_bool(CanopyReport report, const char *parameter, bool value)
{
    _CanopyPropertyValue propval;
    propval.datatype = SDDL_DATATYPE_BOOL;
    propval.val.val_bool = value;
    return _canopy_report_generic(report, parameter, &propval);
}
bool canopy_report_i8(CanopyReport report, const char *parameter, int8_t value)
{
    _CanopyPropertyValue propval;
    propval.datatype = SDDL_DATATYPE_INT8;
    propval.val.val_int8 = value;
    return _canopy_report_generic(report, parameter, &propval);
}
bool canopy_report_u8(CanopyReport report, const char *parameter, uint8_t value)
{
    _CanopyPropertyValue propval;
    propval.datatype = SDDL_DATATYPE_UINT8;
    propval.val.val_uint8 = value;
    return _canopy_report_generic(report, parameter, &propval);
}
bool canopy_report_i16(CanopyReport report, const char *parameter, int16_t value)
{
    _CanopyPropertyValue propval;
    propval.datatype = SDDL_DATATYPE_INT16;
    propval.val.val_int16 = value;
    return _canopy_report_generic(report, parameter, &propval);
}
bool canopy_report_u16(CanopyReport report, const char *parameter, uint16_t value)
{
    _CanopyPropertyValue propval;
    propval.datatype = SDDL_DATATYPE_UINT16;
    propval.val.val_uint16 = value;
    return _canopy_report_generic(report, parameter, &propval);
}
bool canopy_report_i32(CanopyReport report, const char *parameter, int32_t value)
{
    _CanopyPropertyValue propval;
    propval.datatype = SDDL_DATATYPE_INT32;
    propval.val.val_int32 = value;
    return _canopy_report_generic(report, parameter, &propval);
}
bool canopy_report_u32(CanopyReport report, const char *parameter, uint32_t value)
{
    _CanopyPropertyValue propval;
    propval.datatype = SDDL_DATATYPE_UINT32;
    propval.val.val_uint32 = value;
    return _canopy_report_generic(report, parameter, &propval);
}
bool canopy_report_float32(CanopyReport report, const char *parameter, float value)
{
    _CanopyPropertyValue propval;
    propval.datatype = SDDL_DATATYPE_FLOAT32;
    propval.val.val_float32 = value;
    return _canopy_report_generic(report, parameter, &propval);
}
bool canopy_report_float64(CanopyReport report, const char *parameter, double value)
{
    _CanopyPropertyValue propval;
    propval.datatype = SDDL_DATATYPE_FLOAT64;
    propval.val.val_float64 = value;
    return _canopy_report_generic(report, parameter, &propval);
}
bool canopy_report_datetime(CanopyReport report, const char *parameter, const struct tm *datetime)
{
    _CanopyPropertyValue propval;
    propval.datatype = SDDL_DATATYPE_DATETIME;
    memcpy(&propval.val.val_datetime, datetime, sizeof(struct tm));
    return _canopy_report_generic(report, parameter, &propval);
}

bool canopy_send_report(CanopyReport report)
{
    /* construct websocket message */
    CanopyContext canopy = report->ctx;
    RedHashIterator_t iter;
    const void *key;
    size_t keySize;
    const void * value;
    RedJsonObject sddlJson;

    RedJsonObject jsonObj = RedJsonObject_New();
    RED_HASH_FOREACH(iter, report->values, &key, &keySize, &value)
    {
        _CanopyPropertyValue * propVal = (_CanopyPropertyValue *)value;
        switch (propVal->datatype)
        {
            case SDDL_DATATYPE_VOID:
            {
                RedJsonObject_SetNull(jsonObj, key);
                break;
            }
            case SDDL_DATATYPE_STRING:
            {
                RedJsonObject_SetString(jsonObj, key, propVal->val.val_string);
                break;
            }
            case SDDL_DATATYPE_BOOL:
            {
                RedJsonObject_SetBoolean(jsonObj, key, propVal->val.val_bool);
                break;
            }
            case SDDL_DATATYPE_INT8:
            {
                RedJsonObject_SetNumber(jsonObj, key, (double)propVal->val.val_int8);
                break;
            }
            case SDDL_DATATYPE_UINT8:
            {
                RedJsonObject_SetNumber(jsonObj, key, (double)propVal->val.val_uint8);
                break;
            }
            case SDDL_DATATYPE_INT16:
            {
                RedJsonObject_SetNumber(jsonObj, key, (double)propVal->val.val_int16);
                break;
            }
            case SDDL_DATATYPE_UINT16:
            {
                RedJsonObject_SetNumber(jsonObj, key, (double)propVal->val.val_uint16);
                break;
            }
            case SDDL_DATATYPE_INT32:
            {
                RedJsonObject_SetNumber(jsonObj, key, (double)propVal->val.val_int32);
                break;
            }
            case SDDL_DATATYPE_UINT32:
            {
                RedJsonObject_SetNumber(jsonObj, key, (double)propVal->val.val_uint32);
                break;
            }
            case SDDL_DATATYPE_FLOAT32:
            {
                RedJsonObject_SetNumber(jsonObj, key, (double)propVal->val.val_float32);
                break;
            }
            case SDDL_DATATYPE_FLOAT64:
            {
                RedJsonObject_SetNumber(jsonObj, key, propVal->val.val_float64);
                break;
            }
            case SDDL_DATATYPE_DATETIME:
            {
                /*RedJsonObject_SetNumber(jsonObj, key, propVal->val.val_float64);*/
                RedLog_Warn("Datetime reporting not implemented");
                break;
            }
            default:
                assert(!"unimplemented datatype");
        }
    }

    RedJsonObject_SetString(jsonObj, "device_id", report->ctx->uuid);

    sddlJson = sddl_class_json(canopy->sddl);
    RedJsonObject_SetObject(jsonObj, "sddl", sddlJson);

    RedLog_DebugLog("canopy", "Sending Message: %s\n", RedJsonObject_ToJsonString(jsonObj));
    _canopy_ws_write(report->ctx, RedJsonObject_ToJsonString(jsonObj));
    return false;
}

bool canopy_load_sddl(CanopyContext canopy, const char *filename, const char *className)
{
    FILE *fp;
    bool out;
    fp = fopen(filename, "r");
    if (!fp)
    {
        return false;
    }
    out = canopy_load_sddl_file(canopy, fp, className);
    fclose(fp);
    return out;
}

bool canopy_load_sddl_file(CanopyContext canopy, FILE *file, const char *className)
{
    /* Read entire file into memory */
    long filesize;
    char *buffer;
    bool out;
    fseek(file, 0, SEEK_END);
    filesize = ftell(file); 
    fseek(file, 0, SEEK_SET);
    buffer = calloc(1, filesize+1);
    fread(buffer, 1, filesize, file);
    out = canopy_load_sddl_string(canopy, buffer, className);
    free(buffer);
    return out;
}

bool canopy_load_sddl_string(CanopyContext canopy, const char *sddl, const char *className)
{
    SDDLDocument doc;
    SDDLParseResult result;
    SDDLClass cls;

    result = sddl_parse(sddl);
    if (!sddl_parse_result_ok(result))
    {
        RedLog_ErrorLog("canopy", "Failed to parse SDDL");
        return false;
    }

    doc = sddl_parse_result_ref_document(result);

    cls = sddl_document_lookup_class(doc, className);
    if (!cls)
    {
        RedLog_ErrorLog("canopy", "Class not found in SDDL: %s", className);
        return false;
    }

    canopy->sddl = cls;
    return true;
}


bool canopy_event_loop(CanopyContext canopy)
{
    int cnt = 0;
    while (!canopy->quitRequested)
    {
        if (canopy->cb && (cnt % 10 == 0))
        {
            CanopyEventDetails event;
            event = calloc(1, sizeof(CanopyEventDetails_t));
            event->ctx = canopy;
            event->eventType = CANOPY_EVENT_REPORT_REQUESTED;
            event->userData = canopy->cbExtra;
            canopy->cb(canopy, event);
            free(event);
        }
        libwebsocket_service(canopy->ws_ctx, 1000);
        cnt++;
        /* Try to reconnect */
        if (canopy->ws_closed)
        {
            /* we need to tear down entire ws ctx, because there's no other way
             * to dissable the established callbacks (and we don't want them
             * triggering anymore */
            canopy->ws_write_ready = false;
            libwebsocket_context_destroy(canopy->ws_ctx);
            canopy_connect(canopy);
        }
        while (canopy->ws_closed)
        {
            fprintf(stderr, "Trying to reconnect...\n");
            libwebsocket_service(canopy->ws_ctx, 500);
            libwebsocket_service(canopy->ws_ctx, 500);
            libwebsocket_service(canopy->ws_ctx, 500);
            libwebsocket_service(canopy->ws_ctx, 500);
            sleep(1);
            if (!canopy->ws_closed)
                break;
            canopy->ws = libwebsocket_client_connect(
                    canopy->ws_ctx, 
                    canopy->cloudHost, 
                    canopy_get_cloud_port(canopy), 
                    canopy_ws_use_ssl(canopy), 
                    "/echo",
                    canopy->cloudHost,
                    "localhost", /*origin?*/
                    "echo",
                    -1 /* latest ietf version */
            );
            if (!canopy->ws)
            {
                fprintf(stderr, "libwebsocket_client_connect failed\n");
            }
            else
            {
                /* 
                 * This could be a failure, or a success, depending on which callback gets called
                 */
            }
        }
    }
    libwebsocket_context_destroy(canopy->ws_ctx);
    return true;
}

void canopy_quit(CanopyContext canopy)
{
    canopy->quitRequested = true;
}

void canopy_shutdown(CanopyContext canopy)
{
    free(canopy);
}

FILE * canopy_open_config_file(const char* filename)
{
    FILE *fp;
    const char *canopyHome;
    const char *home;

    /* Try: $CANOPY_HOME/<filename> */
    canopyHome = getenv("CANOPY_HOME");
    if (canopyHome)
    {
        RedString fns = RedString_NewPrintf("%s/%s", 2048, canopyHome, filename);
        if (!fns)
            return NULL;
        fp = fopen(RedString_GetChars(fns), "r");
        printf("Using config file: %s\n", RedString_GetChars(fns));
        RedString_Free(fns);
        if (fp)
            return fp;
    }

    /* Try: ~/.canopy/<filename> */
    home = getenv("HOME");
    if (home)
    {
        RedString fns = RedString_NewPrintf("%s/.canopy/%s", 2048, home, filename);
        if (!fns)
            return NULL;
        fp = fopen(RedString_GetChars(fns), "r");
        printf("Using config file: %s\n", RedString_GetChars(fns));
        RedString_Free(fns);
        if (fp)
            return fp;
    }

    /* Try: SYSCONFIGDIR/<filename> */
#ifdef CANOPY_SYSCONFIGDIR
    {
        RedString fns = RedString_NewPrintf("%s/%s", 2048, CANOPY_SYSCONFIGDIR, filename);
        if (!fns)
            return NULL;
        fp = fopen(RedString_GetChars(fns), "r");
        printf("Using config file: %s\n", RedString_GetChars(fns));
        RedString_Free(fns);
        if (fp)
            return fp;
    }
#endif

    /* Try /etc/canopy/<filename> */
    {
        RedString fns = RedString_NewPrintf("/etc/canopy/%s", 2048, filename);
        if (!fns)
            return NULL;
        fp = fopen(RedString_GetChars(fns), "r");
        printf("Using config file: %s\n", RedString_GetChars(fns));
        RedString_Free(fns);
        if (fp)
            return fp;
    }

    return NULL;
}

char * canopy_read_system_uuid()
{
    char *uuidEnv;
    FILE *fp;
    char uuid[37];
    char *out;
    uuidEnv = getenv("CANOPY_UUID");
    if (uuidEnv)
    {
        /* TODO: Verify that it is, in fact, a UUID */
        return RedString_strdup(uuidEnv);
    }

    fp = canopy_open_config_file("uuid");
    if (fp)
    {
        size_t len;
        len = fread(uuid, sizeof(char), 36, fp);
        if (len != 36)
        {
            RedLog_Warn("Expected 36 characters in UUID file", "");
            return NULL;
        }
        uuid[36] = '\0';
        /* TODO: Verify that it is, in fact, a UUID */
        out = RedString_strdup(uuid);
        fclose(fp);
        return out;
    }
    return NULL;
}

const char * canopy_get_web_protocol(CanopyContext ctx)
{
    return ctx->cloudWebProtocol;
}

const char * canopy_get_cloud_host(CanopyContext ctx)
{
    return ctx->cloudHost;
}

uint16_t canopy_get_cloud_port(CanopyContext ctx)
{
    return !strcmp(ctx->cloudWebProtocol, "http") ?
        ctx->cloudHttpPort:
        ctx->cloudHttpsPort;
}

const char *canopy_get_sysconfigdir()
{
#ifdef CANOPY_SYSCONFIGDIR
    return CANOPY_SYSCONFIGDIR
#endif
    return "(was not configured at compilation)\n";
}
