/* WiFi station Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/err.h"
#include "lwip/apps/sntp.h"

#include "epaper-29-dke.h"
#include "epaper_fonts.h"

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

/**************************************************
SETTINGS
**************************************************/
static const char *TAG = " ESP32 CLOCK ";

//SD card settings
#define MOUNT_POINT "/sdcard"
#define SPI_DMA_CHAN    1 // DMA channel to be used by the SPI peripheral

//Wifi settings
#define EXAMPLE_ESP_MAXIMUM_RETRY  1

/* FreeRTOS event group to signal when we are connected*/
/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

//ePaper settings
// Pin definition of the ePaper module
#define MOSI_PIN     13
#define MISO_PIN    -1
#define SCK_PIN     14
#define BUSY_PIN    35
#define DC_PIN      25
#define RST_PIN     26
#define CS_PIN      27
// Color inverse. 1 or 0 = set or reset a bit if set a colored pixel
#define IF_INVERT_COLOR 1


/**************************************************
SOME EXTRA FUNCTIONS for easy working with the epaper
**************************************************/
//For fast bw mode
    epaper_conf_t epaper_conf_fastbw = {
        .busy_pin = BUSY_PIN,
        .cs_pin = CS_PIN,
        .dc_pin = DC_PIN,
        .miso_pin = MISO_PIN,
        .mosi_pin = MOSI_PIN,
        .reset_pin = RST_PIN,
        .sck_pin = SCK_PIN,

        .rst_active_level = 0,
        .busy_active_level = 1,

        .dc_lev_data = 1,
        .dc_lev_cmd = 0,

        .clk_freq_hz = 20 * 1000 * 1000,
        .spi_host = HSPI_HOST,

        .width = EPD_WIDTH,
        .height = EPD_HEIGHT,
        .color_inv = 1,

        .fast_bw_mode = true,
    };

    //For slow full color mode
    epaper_conf_t epaper_conf_slowbwr = {
        .busy_pin = BUSY_PIN,
        .cs_pin = CS_PIN,
        .dc_pin = DC_PIN,
        .miso_pin = MISO_PIN,
        .mosi_pin = MOSI_PIN,
        .reset_pin = RST_PIN,
        .sck_pin = SCK_PIN,

        .rst_active_level = 0,
        .busy_active_level = 1,

        .dc_lev_data = 1,
        .dc_lev_cmd = 0,

        .clk_freq_hz = 20 * 1000 * 1000,
        .spi_host = HSPI_HOST,

        .width = EPD_WIDTH,
        .height = EPD_HEIGHT,
        .color_inv = 1,

        .fast_bw_mode = false,
    };

epaper_handle_t global_fast_epaper = NULL;
epaper_handle_t global_full_epaper = NULL;

epaper_handle_t init_fast_epaper () {
     if (global_full_epaper != NULL) {
        iot_epaper_delete(global_full_epaper, true);
        global_full_epaper = NULL;
    }

    if (global_fast_epaper == NULL) {
        global_fast_epaper = iot_epaper_create(NULL, &epaper_conf_fastbw);
        iot_epaper_set_rotate(global_fast_epaper, E_PAPER_ROTATE_90);
    }

    return global_fast_epaper;
}

epaper_handle_t init_full_epaper () {
    if (global_fast_epaper != NULL) {
        iot_epaper_delete(global_fast_epaper, true);
        global_fast_epaper = NULL;
    }

    if (global_full_epaper == NULL) {
        global_full_epaper = iot_epaper_create(NULL, &epaper_conf_slowbwr);
        iot_epaper_set_rotate(global_full_epaper, E_PAPER_ROTATE_90);
    }

    return global_full_epaper;
}

void clear_screen() {
  
    epaper_handle_t full_epaper = init_full_epaper();
    iot_epaper_clean_paint(full_epaper, WHITE);	//clean the whole screen with WHITE	
    iot_epaper_display_frame(full_epaper);
}

void message_box(char** text,  int line_count) {
    int box_x_offset = 10;
    int box_y_offset = 10;

    int text_x_offset = 10;
    int text_y_offset = 10;

    int text_line_size = 20;
    
    epaper_handle_t fast_epaper = init_fast_epaper ();
   
    iot_epaper_draw_filled_rectangle(fast_epaper, box_x_offset, box_y_offset, epaper_conf_fastbw.height - box_x_offset, epaper_conf_fastbw.width - box_y_offset, WHITE);
    iot_epaper_draw_rectangle(fast_epaper, box_x_offset, box_y_offset, epaper_conf_fastbw.height - box_x_offset, epaper_conf_fastbw.width - box_y_offset, BLACK);    

    for (int i  = 0; i < line_count; i++) {
        iot_epaper_draw_string(fast_epaper, box_x_offset + text_x_offset  , box_y_offset + text_y_offset + i*text_line_size, text[i], &epaper_font_16, BLACK);
    }

    iot_epaper_display_frame(fast_epaper);
}


void Error_Check_Msg(esp_err_t ret, char* text) {
    if (ret != ESP_OK) {

        char *message[1]={'\0'};
        message[0]="ERROR";
        message[1]=text;
        message_box(message, 2);
        
        while (1) {            
            ESP_LOGI(TAG, "ERROR");
            ESP_LOGI(TAG, "See message on screen");
            ESP_LOGI(TAG, "Program stops now");
            vTaskDelay(10000 / portTICK_PERIOD_MS);            
        }
        
    }
}

void Error_Check_Log(esp_err_t ret, char* text) {
    if (ret != ESP_OK) {
        while (1) {            
            ESP_LOGI(TAG, "ERROR");
            ESP_LOGI(TAG, "See message on screen");
            ESP_LOGI(TAG, "Program stops now");
            vTaskDelay(10000 / portTICK_PERIOD_MS);            
        }
        
    }
}


//Wifi connection

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t connect_wifi(char* wifi_ssid, char* wifi_password)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    
    wifi_config_t wifi_config = {};
	wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;    
    strcpy((char*)wifi_config.sta.ssid, wifi_ssid);
    strcpy((char*)wifi_config.sta.password, wifi_password);   

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "connect_wifi finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

   
    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);

     /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s",
                 wifi_ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s",
                 wifi_ssid);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        return ESP_FAIL;
    }

}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32 Clock app");
    char *message[10]={'\0'};
    esp_err_t ret;
    char wifi_ssid[32];
    char wifi_password[64];

    clear_screen();
    message[0]="MESSAGE BOX";
    message[1]="Booting";
    message[2]="Reading settings from SD card...";   
    message_box(message, 3);

    iot_epaper_delete(global_fast_epaper, true);
    global_fast_epaper = NULL;
    
    //Get settings from SD
    //SETTINGS OF MOUNTING

    // Options for mounting the filesystem.    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t* card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.

    //SETTINGS OF SPI
    ESP_LOGI(TAG, "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = MISO_PIN,
        .sclk_io_num = SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    
    Error_Check_Log(spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN), "Failed to initialize bus.");
  
    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = CS_PIN;
    slot_config.host_id = host.slot;

    //MOUNTING
    //Mounting now. if the sdmmc mode is used, the command would be esp_vfs_fat_sdmmc_mount
    Error_Check_Log(esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card), "Failed to mount SD card");

    //READING SSID and PASSWORD
    
    ESP_LOGI(TAG, "Reading file");
    FILE* f = fopen(MOUNT_POINT"/settings.txt", "r");
    if (f == NULL) {
        Error_Check_Log(ESP_FAIL, "Failed to open file for reading");        
    }
    
    fgets(wifi_ssid, sizeof(wifi_ssid), f);
    wifi_ssid[strcspn(wifi_ssid, "\r\n")] = 0;
    fgets(wifi_password, sizeof(wifi_password), f);
    wifi_password[strcspn(wifi_password, "\r\n")] = 0;
    fclose(f);
   

    //UNMOUNTING AND CLOSING SPI
    // All done, unmount partition and disable SDMMC or SPI peripheral
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");

    //deinitialize the bus after all devices are removed
    spi_bus_free(host.slot);

    message[0]="MESSAGE BOX";
    message[1]="Booting";
    message[2]="Connecting to Wifi...";   
    message_box(message, 3);

    // Connecting to Wifi    
    ESP_LOGI(TAG, "CONNECTING TO WIFI");

    //Initialize NVS to store access point name and password
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      Error_Check_Msg(nvs_flash_erase(), "nvs_flash_erase failed");
      Error_Check_Msg(nvs_flash_init(), "nvs_flash_init failed");
    }    


    Error_Check_Msg(connect_wifi(wifi_ssid, wifi_password),"connect_wifi failed");

    ESP_LOGI(TAG, "Wifi connected. Set time now");
    message[0]="MESSAGE BOX";
    message[1]="Wifi connected.";  
    message[2]="Getting time"; 
    message_box(message, 3);

    // Set timezone to Eastern Standard Time and print local time
    //https://github.com/espressif/esp-idf/blob/master/examples/protocols/sntp/README.md
    //https://man7.org/linux/man-pages/man3/setenv.3.html
    //https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
    //https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
    setenv("TZ", "AEST-10AEDT,M10.1.0,M4.1.0/3", 1);    //Sydney time
    tzset();     

    time_t now;
    struct tm timeinfo; //https://www.cplusplus.com/reference/ctime/tm/
    char strftime_buf[64];

    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Sydney is: %s", strftime_buf);

    
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Time is not set yet. Getting time over NTP from pool.ntp.org.");
        
        ESP_LOGI(TAG, "Initializing SNTP");
        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, "pool.ntp.org");
        sntp_init();

        // try getting the correct time    
        int retry = 0;
        const int retry_count = 10;
        while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
            ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            time(&now);
            localtime_r(&now, &timeinfo);
        }
    }
    
    if (timeinfo.tm_year < (2016 - 1900)) {
        Error_Check_Msg(ESP_FAIL, "Cant get NTP time");
    }
    
    ESP_LOGI(TAG, "got NTP time successfully");
    
    //Show time on the ePaper display
    int  min=-1, sec=-1;    
    char hour_text[4];
    char min_text[4]; 
    char sec_text[4]; 
    char *month_text[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    char *day_text[7] = {"MON", "TUE", "WED", "THUR", "FRI", "SAT", "SUN"};
    char day_mon_year_text[30];    

    //Main loop to show time on the epaper display
    while(1){
        time(&now);
        localtime_r(&now, &timeinfo);

        //Full update every minute to avoid permanently destroying the display 
        if (min != timeinfo.tm_min) {
        
            epaper_handle_t full_epaper = init_full_epaper();

            iot_epaper_clean_paint(full_epaper, WHITE);	//clean the whole screen with WHITE			    

            sprintf(day_mon_year_text, "%s, %d %s %d",  day_text[timeinfo.tm_wday],timeinfo.tm_mday, month_text[timeinfo.tm_mon], timeinfo.tm_year-100+2000 );
            iot_epaper_draw_string(full_epaper, 10, 5, day_mon_year_text, &epaper_font_24, RED);

             sprintf(hour_text, "%02d", timeinfo.tm_hour); 
            iot_epaper_draw_string(full_epaper, 0, 40, hour_text, &epaper_font_60, RED);  
            iot_epaper_draw_string(full_epaper, 75, 25, ":", &epaper_font_60, RED);  
            
            sprintf(min_text, "%02d", timeinfo.tm_min); 
            iot_epaper_draw_string(full_epaper, 100, 40, min_text, &epaper_font_60, RED); 
            iot_epaper_draw_string(full_epaper, 175, 25, ":", &epaper_font_60, RED); 

            iot_epaper_display_frame(full_epaper);
        } 
        
        //We use else if here so timeinfo will be updated again before we print the sec
        else if (sec != timeinfo.tm_sec) {        
            epaper_handle_t fast_epaper = init_fast_epaper ();
            iot_epaper_clean_paint(fast_epaper, WHITE);
            sprintf(sec_text, "%02d", timeinfo.tm_sec);            
            iot_epaper_draw_string(fast_epaper, 200, 40, sec_text, &epaper_font_60, BLACK);
            iot_epaper_display_frame(fast_epaper);
        }
        
        min = timeinfo.tm_min;
        sec  = timeinfo.tm_sec;
    }
    
 
}