#include <stdio.h>
// #include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bget.h"
#include "luat_base.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#define LUAT_HEAP_SIZE (64*1024)
uint8_t luavm_heap[LUAT_HEAP_SIZE] = {0};


void timer_callback(void* args){
    printf("from LuatOS main timer\n");
}

void app_main(void)
{
    //xTimerCreate("luat_timer", 1000 / portTICK_RATE_MS, 1, NULL, timer_callback);
    bpool(luavm_heap, LUAT_HEAP_SIZE);  // lua vm��Ҫһ���ڴ������ڲ�����, �����׵�ַ����С.
    luat_main();      // luat_main��LuatOS�������, �÷���ͨ�����᷵��.
}
