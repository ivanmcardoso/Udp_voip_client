//Udp client
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <stdio.h>
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "esp_timer.h"
#include <driver/adc.h>
#include "driver/i2s.h"

#define SSID 						"Katia Cardoso"
#define PASSWORD					"jnc196809"
#define HOST_IP_ADDR 				"10.0.0.101"
#define PORT 						8000
#define SAMPLE_RATE 				8000
#define BUFFER_MAX  				8000

int16_t audioBuffer[BUFFER_MAX];
int time_before = 0,
	j = 0,
	wifi_is_connected = 0;

static EventGroupHandle_t s_wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
/*
static void send_all(int sock, const void *vbuf, size_t size_buf, struct sockaddr_in source_addr)
{
	const char *buf = (char*)vbuf;
	int send_size;
	size_t size_left;
	const int flags = 0;
	socklen_t socklen = sizeof(source_addr);

	size_left = size_buf;

	while(size_left > 0 )
	{
		if((send_size = sendto(sock, buf,size_left,flags, (struct sockaddr *)&source_addr, socklen)) == -1)
		{
			printf("Erro ao enviar\n");
			break;
		}
		if(send_size == 0)
		{
			printf("envio completp\n");
			break;
		}

		size_left -= send_size;
		buf +=send_size;
	}
	return;
}
*/
static void udp_client_task(void *pvParameters)
{
	 char addr_str[128];
    int addr_family;
    int ip_protocol;

    while(1){
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);

	//	send_all(sock,(void*)&audioBuffer, sizeof(audioBuffer), dest_addr);
    	const int flags = 0;

    	socklen_t socklen = sizeof(dest_addr);

    	char* payload = "Juntos e shallow now";
    	int len = sendto(sock,payload, strlen(payload),flags, (struct sockaddr *)&dest_addr, socklen);
    	printf("Port: %d len = %d\n", (int)PORT, len);
    	if(len == strlen(payload)){
    		printf("enviado\n");
    	}
    	else if(len == -1){
    		printf("erro ao enviar\n");
    	}
		shutdown(sock, 0);
        close(sock);
        vTaskDelete(NULL);
    }
}

static void periodic_timer_callback(void * arg)
{
	int readValue = adc1_get_raw(ADC1_CHANNEL_0);
	audioBuffer[j] = readValue*1.5;
	j++;
	if(j>=BUFFER_MAX){
		j = 0;
		printf("Coletado\n");
			//printf("time: %lld\n",(esp_timer_get_time()-time_before)/1000);
		xTaskCreatePinnedToCore(udp_client_task, "udp_client", 4096, NULL, 5, NULL,1);
	}

}

static void create_timer(){

    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &periodic_timer_callback,
            .name = "periodic"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    time_before = esp_timer_get_time();
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, (1000000/SAMPLE_RATE)));
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
        printf("conectado ao wifi\n");
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
    	gpio_set_level(GPIO_NUM_2, 1);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        create_timer();
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        {
        	gpio_set_level(GPIO_NUM_2, 0);
            do {
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                vTaskDelay(300/portTICK_PERIOD_MS);
            }while(esp_wifi_connect() != ESP_OK);

            break;
        }
    default:
        break;
    }
    return ESP_OK;
}
//configuração do wifi com station
void setup_wifi()
{
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = SSID,
            .password = PASSWORD
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

}
//configuração dos pinos de entrada e de saída
static void pins_Setup(){

    gpio_pad_select_gpio(GPIO_NUM_2);
	gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);

	adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_DB_0);

}

void app_main(void)
{
	pins_Setup();
	nvs_flash_init();
	setup_wifi();

}
