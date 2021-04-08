/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
// #include "esp_system.h"
// #include "esp_spi_flash.h"
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bget.h"
#include "luat_base.h"

#define LUAT_HEAP_SIZE (1024)
uint8_t luavm_heap[LUAT_HEAP_SIZE] = {0};

void app_main(void)
{
    bpool(luavm_heap,LUAT_HEAP_SIZE);  // lua vm��Ҫһ���ڴ������ڲ�����, �����׵�ַ����С.
    luat_main();      // luat_main��LuatOS�������, �÷���ͨ�����᷵��.
}
