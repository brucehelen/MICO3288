#include "pb.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "sitewhere.h"
#include "sitewhere-arduino.pb.h"


/****************************************************/

#include "mico.h"
#include "MicoAES.h"
#include "libemqtt.h"
#include "custom.h"
#include "micokit_ext.h"

#define baseEvents_log(M, ...) custom_log("BaseEvents", M, ##__VA_ARGS__)
#define baseEvents_log_trace() custom_log_trace("BaseEvents")

extern mqtt_broker_handle_t broker_mqtt;
//mico_Context_t *context;
/*****************************************************/
// Update these with values suitable for your network.
//byte mac[]  = { 0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xED };
//byte mqtt[] = { 192, 168, 1, 68 };
/** Callback function header */
void callback(char* topic, byte* payload, unsigned int length);

/** Connectivity */
//PubSubClient mqttClient(mqtt, 1883, callback, ethClient);

/** Message buffer */
uint8_t buffer[300];

/** Keeps up with whether we have registered */
bool registered = false;

/** Timestamp for last event */
//uint32_t lastEvent = 0;

/** MQTT client name */
char* clientName = "MiCOKit";

/** Unique hardware id for this device */
char hardwareId[HARDWARE_ID_SIZE] = {"MiCOKit-3288_MK3288_1_000000"};

/** Device specification token for hardware configuration */
char* specificationToken = "7f496af1-6a20-4d08-9fe0-ed59d50b9940";

/** Outbound MQTT topic */
char* outbound = "SiteWhere/input/protobuf";

/** Inbound custom command topic */
char Command1[COMMAND1_SIZE] = "SiteWhere/commands/MiCOKit-3288_MK3288_1_000000";

/** Inbound system command topic */
char System1[SYSTEM1_SIZE] = "SiteWhere/system/MiCOKit-3288_MK3288_1_000000";


const pb_field_t ArduinoCustom_RGB_fields[4] = {
  PB_FIELD2(  1, INT32  , REQUIRED, STATIC  , FIRST, ArduinoCustom_RGB, hues, hues, 0),
  PB_FIELD2(  2, INT32  , REQUIRED, STATIC  , OTHER, ArduinoCustom_RGB, saturation, hues, 0),
  PB_FIELD2(  3, INT32  , REQUIRED, STATIC  , OTHER, ArduinoCustom_RGB, brightness, saturation, 0),
  PB_LAST_FIELD
};

/** Handle the 'ping' command */
void handlePing(ArduinoCustom_ping ping, char* originator) {
  unsigned int len = 0;
  memset(buffer,0,300);
  if (len = sw_acknowledge(hardwareId, "Ping received.", buffer, sizeof(buffer), originator)) {
    mqtt_publish(&broker_mqtt,outbound,(char*)buffer,len,0);
  }
}

void handleMotor(ArduinoCustom_motor motor, char* originator) {
  unsigned int len = 0;
  memset(buffer,0,300);
  if (len = sw_acknowledge(hardwareId, "motor received.", buffer, sizeof(buffer), originator)) {
    mqtt_publish(&broker_mqtt,outbound,(char*)buffer,len,0);
  }
}

/** Handle the 'serialPrintln' command */
void handleSerialPrintln(ArduinoCustom_serialPrintln serialPrintln,char* originator) {
  unsigned int len = 0;
  memset(buffer,0,300);
  OLED_Init();
  char oled_show_line[OLED_DISPLAY_MAX_CHAR_PER_ROW+1] = {'\0'};
  baseEvents_log("%s",serialPrintln.message);
  snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "%s",serialPrintln.message);
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, (uint8_t*)oled_show_line);
  if (len = sw_acknowledge(hardwareId, "Message sent to Serial.println().", buffer, sizeof(buffer), originator)) {
    mqtt_publish(&broker_mqtt,outbound,(char*)buffer,len,0);
  }
}

device_func cmdd[50];
/** Handle the 'testEvents' command */
void handleTestEvents(ArduinoCustom_RGB testEvents, char* originator) {
  unsigned int len = 0;
  memset(buffer,0,300);
  //  if (len = sw_location(hardwareId, 33.755f, -84.39f, 0.0f, NULL, buffer, sizeof(buffer), originator)) {
  //     mqtt_publish(&broker_mqtt,outbound,buffer,len,0);
  //  }
  memset(cmdd,0,sizeof(cmdd));
  cmdd[0].engine.temp="hues";
  cmdd[0].engine.value = testEvents.hues;
  
  cmdd[1].engine.temp="saturation";
  cmdd[1].engine.value = testEvents.saturation;
  
  cmdd[2].engine.temp="brightness";
  cmdd[2].engine.value = testEvents.brightness;
  
  if (len = sw_measurement(hardwareId,&cmdd[0],3,NULL, buffer, sizeof(buffer), originator)) {
    mqtt_publish(&broker_mqtt,outbound,(char*)buffer,len,0);
  }
  //  if (len = sw_alert(hardwareId, "engine.overheat", "The engine is overheating!", NULL, buffer, sizeof(buffer), originator)) {
  //     mqtt_publish(&broker_mqtt,outbound,buffer,len,0);
  //  }
}

ArduinoCustom_RGB RGB;
/** Handle a command specific to the specification for this device */
void handleSpecificationCommand(byte* payload, unsigned int length) {
  ArduinoCustom__Header header;
  
  memset(buffer,0,300);
  
  pb_istream_t stream = pb_istream_from_buffer(payload, length);
  if (pb_decode_delimited(&stream, ArduinoCustom__Header_fields, &header)) {
    baseEvents_log("Decoded header for custom command.");
    if (header.command == 1) {
      //        ArduinoCustom_motor motor;
      //        if (pb_decode_delimited(&stream, ArduinoCustom_serialPrintln_fields, &motor)) {
      //            dc_motor_set(atoi(motor.message));
      //        }
      ArduinoCustom_RGB_SW rgbled_sw;
      if (pb_decode_delimited(&stream, ArduinoCustom__Header_fields, &rgbled_sw)) {
        if(rgbled_sw.onoff){
          if ( (RGB.brightness > 0) && (RGB.saturation > 0)) {
            hsb2rgb_led_open(RGB.hues,RGB.saturation,RGB.brightness);
          }
          else{
            hsb2rgb_led_open(120, 100, 100);
          }
        }else{
          hsb2rgb_led_close();
        }
      }
    }
    if (header.command == 2) {
      if (pb_decode_delimited(&stream, ArduinoCustom_RGB_fields, &RGB)) {
        hsb2rgb_led_open(RGB.hues,RGB.saturation,RGB.brightness);
        handleTestEvents(RGB, header.originator);
      }
    }
    else if (header.command == 3) {
      ArduinoCustom_ping ping;
      if (pb_decode_delimited(&stream, ArduinoCustom_ping_fields, &ping)) {
        handlePing(ping, header.originator);
      }
    } else if (header.command == 4) {
      ArduinoCustom_testEvents testEvents;
      if (pb_decode_delimited(&stream, ArduinoCustom_testEvents_fields, &testEvents)) {
        // handleTestEvents(testEvents, header.originator);
      }
    } else if (header.command == 5) {
      ArduinoCustom_serialPrintln serialPrintln;
      if (pb_decode_delimited(&stream, ArduinoCustom_serialPrintln_fields, &serialPrintln)) {
        handleSerialPrintln(serialPrintln,header.originator);
      }
    }
  }
}

/** Handle a system command */
void handleSystemCommand(byte* payload, unsigned int length) {
  Device_Header header;
  pb_istream_t stream = pb_istream_from_buffer(payload, length);
  
  // Read header to find what type of command follows.
  if (pb_decode_delimited(&stream, Device_Header_fields, &header)) {
    
    // Handle a registration acknowledgement.
    if (header.command == Device_Command_REGISTER_ACK) {
      Device_RegistrationAck ack;
      if (pb_decode_delimited(&stream, Device_RegistrationAck_fields, &ack)) {
        if (ack.state == Device_RegistrationAckState_NEW_REGISTRATION) {
          baseEvents_log("Registered new device.");
          registered = true;
        } else if (ack.state == Device_RegistrationAckState_ALREADY_REGISTERED) {
          baseEvents_log("Device was already registered.");
          registered = true;
        } else if (ack.state == Device_RegistrationAckState_REGISTRATION_ERROR) {
          baseEvents_log("Error registering device.");
        }
      }
    }
  } else {
    baseEvents_log("Unable to decode system command.");
  }
}
