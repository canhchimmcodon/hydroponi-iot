#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_event_loop.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"

#include "wifi_manager.h"

#define SSID "NCT"
#define PASS "dccndccn"

EventGroupHandle_t wifi_event_group;

const int CONNECTED_BIT = BIT0;

extern bool wifi_connected;

esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
	case SYSTEM_EVENT_STA_START:
	  esp_wifi_connect();
	  break;
	case SYSTEM_EVENT_STA_GOT_IP:
	  xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
	  wifi_connected = true;
	  break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
	  wifi_connected = false;
	  esp_wifi_connect();
	  xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
	  break;
	default:
	  break;
    }
    return ESP_OK;
}

void wifi_manager() 
{
	tcpip_adapter_init();

	wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL) );

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = SSID,
            .password = PASS,
			.bssid_set = false
        },
    };

    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK( esp_wifi_start() );
	
	xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
	
}
