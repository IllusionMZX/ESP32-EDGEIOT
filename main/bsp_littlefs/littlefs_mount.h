#ifndef LITTLEFS_MOUNT_H
#define LITTLEFS_MOUNT_H

#include "esp_err.h"
#include "esp_log.h"
#include "esp_littlefs.h"
esp_err_t mount_storage_littlefs(const char* base_path);

#endif // LITTLEFS_MOUNT_H