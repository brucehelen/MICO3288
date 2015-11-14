/**
******************************************************************************
* @file    user_main.c 
* @author  Eshen Wang
* @version V1.0.0
* @date    14-May-2015
* @brief   user main tasks in user_main thread.
******************************************************************************
* @attention
*
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
* TIME. AS A RESULT, MXCHIP Inc. SHALL NOT BE HELD LIABLE FOR ANY
* DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
* FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
* CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*
* <h2><center>&copy; COPYRIGHT 2014 MXCHIP Inc.</center></h2>
******************************************************************************
*/ 

#include "mico.h"
#include "MiCOAppDefine.h"

#include "json_c/json.h"
#include "rgb_led/hsb2rgb_led.h"
#include "lcd/oled.h"

/* User defined debug log functions
 * Add your own tag like: 'USER', the tag will be added at the beginning of a log
 * in MICO debug uart, when you call this function.
 */
#define user_log(M, ...) custom_log("USER", M, ##__VA_ARGS__)
#define user_log_trace() custom_log_trace("USER")


/* user main function, called by AppFramework after system init done && wifi
 * station on in user_main thread.
 */
OSStatus user_main( app_context_t * const app_context )
{
  user_log_trace();
  OSStatus err = kUnknownErr;
  require(app_context, exit);
  
  hsb2rgb_led_init();  // rgb led init
  
  while(1){
    mico_thread_sleep(1);
    
    // update info on LCD
    OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, "SiteWhere demo  ");  // clean line2
    OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, "RGB LED control ");  // show led cmd
    OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, "                ");  // clean line4
  }

exit:
  user_log("ERROR: user_main exit with err=%d", err);
  return err;
}
