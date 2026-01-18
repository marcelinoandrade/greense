/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_check.h"

#include "linux/videodev2.h"
#include "esp_video_isp_ioctl.h"
#include "esp_video_device.h"

static const char *TAG = "app_isp";

static int video_fd = -1;
static int cam_fd = -1;

esp_err_t app_isp_init(int cam_fd_param)
{
    video_fd = open(ESP_VIDEO_ISP1_DEVICE_NAME, O_RDWR);
    cam_fd = cam_fd_param;

    return ESP_OK;
}

esp_err_t app_isp_set_contrast(uint32_t percent)
{
    uint32_t contrast_val = 0xff * percent / 100;

    esp_err_t ret;
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];

    controls.ctrl_class = V4L2_CID_USER_CLASS;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_CONTRAST;
    control[0].value     = contrast_val;

    ret = ioctl(video_fd, VIDIOC_S_EXT_CTRLS, &controls);
    if (ret != 0) {
        ESP_LOGE(TAG, "failed to set contrast");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t app_isp_set_saturation(uint32_t percent)
{
    uint32_t sat_val = 0xff * percent / 100;

    esp_err_t ret;
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];

    controls.ctrl_class = V4L2_CID_USER_CLASS;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_SATURATION;
    control[0].value     = sat_val;
    ret = ioctl(video_fd, VIDIOC_S_EXT_CTRLS, &controls);
    if (ret != 0) {
        ESP_LOGE(TAG, "failed to set saturation");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t app_isp_set_brightness(uint32_t percent)
{
    int32_t bright_val = 0xff * percent / 100 - 127;

    esp_err_t ret;
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];

    controls.ctrl_class = V4L2_CID_USER_CLASS;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_BRIGHTNESS;
    control[0].value     = bright_val;
    ret = ioctl(video_fd, VIDIOC_S_EXT_CTRLS, &controls);
    if (ret != 0) {
        ESP_LOGE(TAG, "failed to set brightness");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t app_isp_set_hue(uint32_t percent)
{
    uint32_t hue_val = 360 * percent / 100;

    esp_err_t ret;
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];

    controls.ctrl_class = V4L2_CID_USER_CLASS;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_HUE;
    control[0].value     = hue_val; 
    ret = ioctl(video_fd, VIDIOC_S_EXT_CTRLS, &controls);
    if (ret != 0) {
        ESP_LOGE(TAG, "failed to set hue");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Set exposure time
 * 
 * @param exposure_us Exposure time in microseconds (e.g., 500 = 1/2000s, 1000 = 1/1000s, 2000 = 1/500s)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t app_isp_set_exposure(uint32_t exposure_us)
{
    if (cam_fd < 0) {
        ESP_LOGE(TAG, "Camera file descriptor not initialized");
        return ESP_FAIL;
    }

    esp_err_t ret;
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];

    controls.ctrl_class = V4L2_CID_CAMERA_CLASS;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_EXPOSURE_ABSOLUTE;
    // V4L2_CID_EXPOSURE_ABSOLUTE uses units of 100us according to ESP-IDF
    control[0].value    = exposure_us / 100;

    ret = ioctl(cam_fd, VIDIOC_S_EXT_CTRLS, &controls);
    if (ret != 0) {
        ESP_LOGE(TAG, "failed to set exposure time: %lu us", exposure_us);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Exposure set to %lu us (1/%lu s)", 
             exposure_us, (exposure_us > 0) ? (1000000 / exposure_us) : 0);
    return ESP_OK;
}