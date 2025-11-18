#ifndef APP_HTTP_H
#define APP_HTTP_H

#include "app_thermal.h"

bool app_http_send_thermal_data(const float temps[APP_THERMAL_TOTAL]);
bool app_http_check_connectivity(const char *url);

#endif // APP_HTTP_H

