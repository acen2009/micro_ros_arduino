#include <micro_ros_arduino.h>

#include <stdio.h>
#include <RTCZero.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rmw_microros/rmw_microros.h>

rclc_support_t support;
rcl_allocator_t allocator;

/* Create an rtc object */
RTCZero rtc;

#define LED_PIN 13

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){error_loop();}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}

const int timeout_ms = 1000;
static int64_t time_ms;
static uint32_t time_seconds;
char time_str[25];

void error_loop(){
  while(1){
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(100);
  }
}

void setup() {
  // Ethernet MAC address
  byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
  
  // Configure micro-ROS UDP transport
  IPAddress qec_ip(192, 168, 3, 202);
  IPAddress agent_ip(192, 168, 3, 59);
  set_microros_native_ethernet_udp_transports(mac, qec_ip, agent_ip, 9999);
  Serial.begin(3000000); // Configure debug serial
  rtc.begin(); // initialize RTC
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  
  
  delay(2000);

  allocator = rcl_get_default_allocator();

  //create init_options
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));
}

void loop() {
  // Synchronize time
  RCCHECK(rmw_uros_sync_session(timeout_ms));
  time_ms = rmw_uros_epoch_millis(); 
  
  if (time_ms > 0)
  {
    time_seconds = time_ms/1000;
    rtc.setEpoch(time_seconds); 
    sprintf(time_str, "%02d.%02d.%04d %02d:%02d:%02d.%03d", rtc.getDay(), rtc.getMonth(), rtc.getYear(), rtc.getHours(), rtc.getMinutes(), rtc.getSeconds(), time_ms % 1000);

    Serial.print("Agent date: ");
    Serial.println(time_str);  
  }
  else
  {
    Serial.print("Session sync failed, error code: ");
    Serial.println((int) time_ms);  
  }
  
  delay(1001);
}
