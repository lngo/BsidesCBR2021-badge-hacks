/* 2.9" DKE ePaper Driver Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "esp_log.h"

#include "epaper-29-dke.h"
#include "epaper_fonts.h"

static const char *TAG = "ePaper Example";

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

void e_paper_task(void *pvParameter)
{
    epaper_handle_t device = NULL;

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

    
 
    //Fast mode by default
    device = NULL;
 
    int fast_refresh_count = 0;
    char count_str[12];

    int TIME_TO_FULL_REFRESH = 60;
    fast_refresh_count = TIME_TO_FULL_REFRESH; // This is to force a proper clean in the beginning.
    while(1){
        
		if (fast_refresh_count  == TIME_TO_FULL_REFRESH) {
            if (device != NULL) {
                iot_epaper_delete(device, true); 
            }          
            device = iot_epaper_create(NULL, &epaper_conf_slowbwr);
            iot_epaper_set_rotate(device, E_PAPER_ROTATE_90);
           

            iot_epaper_clean_paint(device, WHITE);	//clean the whole screen with WHITE			
		
            iot_epaper_draw_string(device, 75, 10, "EPAPER DEMO", &epaper_font_20, RED);
            iot_epaper_draw_string(device, 40, 35, "DEPG0290RHS75BF6CP-H0", &epaper_font_16, BLACK);
            iot_epaper_draw_line(device, 10, 55 , 150, 70, BLACK); //For horizontal or vertical lines, dont use this function.		
            iot_epaper_draw_filled_rectangle(device, 160, 55, 280, 70, RED);		
            iot_epaper_draw_filled_circle( device, 80, 100, 50, BLACK); //This circle is intentionally written to expand beyond the boundary and overlap the above line, just for testing.		
            iot_epaper_draw_circle( device, 200, 100, 20, RED);	

            iot_epaper_display_frame(device);

           
            fast_refresh_count = 0;
            iot_epaper_delete(device, true);
            device = NULL;

            vTaskDelay(5000 / portTICK_PERIOD_MS); //Delay for 5 seconds to show the display with RED prints.
           
        } else {
            if  (device == NULL) {
                device = iot_epaper_create(NULL, &epaper_conf_fastbw);
                iot_epaper_set_rotate(device, E_PAPER_ROTATE_90);
            }

            memset(count_str, 0x00, sizeof(count_str));          
            sprintf(count_str, "%d", fast_refresh_count);

            iot_epaper_clean_paint(device, WHITE);	//clean the whole screen buffer with WHITE	
            iot_epaper_draw_string(device, 25, 35, "Fast refresh count=", &epaper_font_16, BLACK);
            iot_epaper_draw_string(device, 255, 35, count_str, &epaper_font_16, BLACK);
            
            //This circle is intentionally written to expand beyond the boundary and overlap the above line, just for testing.		
            iot_epaper_draw_filled_circle( device, 150, 90, (3 - fast_refresh_count % 3)*10 , BLACK); 
           

            iot_epaper_display_frame(device);

            fast_refresh_count++;

        }
		
		
		vTaskDelay(1000 / portTICK_PERIOD_MS);	
        
    }

    iot_epaper_delete(device, true); 
}

void app_main()
{
    ESP_LOGI(TAG, "Starting example");
    xTaskCreate(&e_paper_task, "epaper_task", 4 * 1024, NULL, 5, NULL);
}