#include <stdio.h>
// #include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "esp_system.h"
// #include "esp_spi_flash.h"
#include "bget.h"
#include "luat_base.h"

#define LUAT_HEAP_SIZE (1024*100)
uint8_t luavm_heap[LUAT_HEAP_SIZE] = {0};

void app_main(void)
{
    bpool(luavm_heap,LUAT_HEAP_SIZE);  // lua vm��Ҫһ���ڴ������ڲ�����, �����׵�ַ����С.
    luat_main();      // luat_main��LuatOS�������, �÷���ͨ�����᷵��.
}