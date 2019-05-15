#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>

#include "esp_wifi.h"
#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include <hwcrypto/aes.h>

#include "mqttNDH.h"

#include "cJSON.h"
#include "actuatorNDH.h"

#include "dht22.h"

#include "crypto/base64.h"

const char *TAG = "MQTT";

const char *broker = "mqtt://broker.hivemq.com";

extern bool mqtt_connected;
extern bool wifi_connected;
extern bool identified;

esp_mqtt_client_handle_t client;

esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event) {
  esp_mqtt_client_handle_t client = event->client;
 
  char subcribe_topic[1024] ;
  char command[2048] ;

  switch (event->event_id) {
	
  case MQTT_EVENT_CONNECTED:
	ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
	mqtt_connected = true;

	esp_mqtt_client_subscribe(client, "MQTT_IDENTIFY_REPLY_NDH", 0);
	ESP_LOGI(TAG, "Subscribe identify server successful!");

	esp_mqtt_client_subscribe(client, "MQTT_CONTROL_NDH", 0);
	ESP_LOGI(TAG, "Subscribe control server successful!");

	if ( !identified ) {
	  publish_iden();
	}
	
	break;

  case MQTT_EVENT_SUBSCRIBED:
	break;

  case MQTT_EVENT_UNSUBSCRIBED:
	break;

  case MQTT_EVENT_PUBLISHED:
	break;
	
  case MQTT_EVENT_DISCONNECTED:
	ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
	mqtt_connected = false;
	break;
	
  case MQTT_EVENT_DATA: 
	ESP_LOGI(TAG, "MQTT_EVENT_DATA");
	
	sprintf(subcribe_topic, "%.*s", event->topic_len, event->topic);
	sprintf(command, "%.*s", event->data_len, event->data);

	//printf("%s\n",subcribe_topic);
	//printf("%s\n", command);

	if ( strcmp(subcribe_topic, "MQTT_IDENTIFY_REPLY_NDH" ) == 0) {
	  cJSON *json = cJSON_Parse(command);
	  const cJSON *item = cJSON_GetObjectItem(json, "message");
	  char *text = item->valuestring;
	  //	  printf("%s\n",text);
	  if ( strcmp(text, "OK") == 0 ) {
		ESP_LOGI(TAG,"Check identify!\n");
		identified = true;
	  } else {
		vTaskDelay(pdMS_TO_TICKS(20 * 1000));
		publish_iden();
	  }
	  
	}
	
	if (strcmp(subcribe_topic, "MQTT_CONTROL_NDH") == 0 && identified == true) {
	  turn_pump_on_with_command(command);
	}
	
	break;
  
  case MQTT_EVENT_ERROR:
	ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
	break;
  }
  return ESP_OK;
}

/*
esp_err_t mqtt_event_handler_2(esp_mqtt_event_handle_t event) {
  esp_mqtt_client_handle_t client = event->client;

  if ( event->event_id == MQTT_EVENT_CONNECTED ) {
	printf("%d\n",getpid());
	mqtt_connected = true;
	ESP_LOGI(TAG, "Check identify ...");

	char *topic = "MQTT_IDENTIFY_NDH";
	cJSON *root = cJSON_CreateObject();
	cJSON_AddStringToObject(root, "deviceId", "dev1");
	cJSON_AddStringToObject(root, "username", "dev1");
	cJSON_AddStringToObject(root, "password", "dev1");
	char *text = cJSON_PrintUnformatted(root);
	  
	while (identified == false) {  
	  esp_mqtt_client_publish(client, topic, text, 0, 0, 0);
	  ESP_LOGI(TAG, "Sent identify message successful !");
	  vTaskDelay(pdMS_TO_TICKS(20 * 1000));
	}
  }
  
  return ESP_OK;
}

*/


void publish_data_to_broker() {
  while(1) {
	if ( !wifi_connected ) {
	  ESP_LOGI(TAG, "WIFI is not connected!\n");
	  vTaskDelay(pdMS_TO_TICKS(1000*60));
	}
	else if ( !mqtt_connected ) {
	  ESP_LOGI(TAG, "MQTT_Server is not connected!\n");
	  vTaskDelay(pdMS_TO_TICKS(1000*60));
	}
	else if ( !identified ) {
	  ESP_LOGI(TAG, "Not identified yet!\n");
	  vTaskDelay(pdMS_TO_TICKS(1000*60));
	}
	else {
	  for(int i = 0; i < 10; i++) {
		readDHT();
		
		cJSON *root = NULL;
		root = cJSON_CreateObject();
		cJSON_AddStringToObject(root, "deviceId", "dev1");
		cJSON_AddNumberToObject(root, "temperature", getTemperature() );
		cJSON_AddNumberToObject(root, "humidity", getHumidity() );
		cJSON_AddNumberToObject(root, "moisture", (float) read_soil_moisture() / 4095 );
		char *text = cJSON_PrintUnformatted(root);
		ESP_LOGI(TAG,"Sent data: %s\n",text);
		esp_mqtt_client_publish(client,"MQTT_COLLECT_NDH",text,0,0,0);
		//vTaskDelay(pdMS_TO_TICKS(1000));
	  }
	  vTaskDelay(pdMS_TO_TICKS(1000*60*10));
	}
  }
  
}

void mqtt_app_start(void) {
  esp_mqtt_client_config_t mqtt_cfg = {
	.uri = broker,
	.event_handle = mqtt_event_handler,
	.port = 1883,
  };

  client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_start(client);

  vTaskDelay(5000/portTICK_PERIOD_MS);
  
  xTaskCreate(publish_data_to_broker, "Name", 4096, NULL, 5, NULL);
}

void publish_iden() {  
  char *topic = "MQTT_IDENTIFY_NDH";
  cJSON *root = cJSON_CreateObject();

  char *plaintext = "howtogetagoodpassword";
  size_t len;
  unsigned char *encrypted = base64_encode( (const unsigned char*)plaintext, strlen(plaintext) , &len );

  char result[1024];
  sprintf( result, "%.*s", len, encrypted );
  result[len-1] = '\0';
  //printf("%s\n", plaintext);
  //printf("%s\n", result);

  
  cJSON_AddStringToObject(root, "deviceId", "dev1");
  cJSON_AddStringToObject(root, "username", "dev1");
  cJSON_AddStringToObject(root, "password", (const char*)result);
  char *text = cJSON_PrintUnformatted(root);
	  
  esp_mqtt_client_publish(client, topic, text, 0, 0, 0);
  ESP_LOGI(TAG, "Sent identify message successful !");
  vTaskDelay(pdMS_TO_TICKS(20 * 1000));
}
