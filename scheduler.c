#include "scheduler.h"

extern SemaphoreHandle_t semaphore; //because it is defined into main.c file
extern SubFrame_t major_frame[NUM_SUB_FRAMES];
static int8_t n_current_sf = 0;    //to count the current subframe number for eache major frame

//function to handle sub-frame frame in the major-frame
void dispatcher (){
    for(;;) {
        if(xSemaphoreTake(semaphore, portMAX_DELAY) == pdTRUE) {
            // execute hrt task if exist
            if (major_frame[n_current_sf].hrt_task != NULL) {
                vTaskResume(major_frame[n_current_sf].hrt_task);
            }
            // execute all SRT tasks associated to this sub-frame
            for (uint8_t i = 0; i < major_frame[n_current_sf].num_srt_tasks; i++) {
                if (major_frame[n_current_sf].srt_tasks[i] != NULL) {
                    vTaskResume(major_frame[n_current_sf].srt_tasks[i]);
                }
            }
        }

        //increase counter of executed subframe until complete major frame
        n_current_sf++;
        if(n_current_sf > NUM_SUB_FRAMES - 1) {
            n_current_sf = 0;
        }
    }

}

void vConfigureScheduler(){}

  