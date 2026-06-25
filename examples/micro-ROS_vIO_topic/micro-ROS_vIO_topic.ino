/**
 * @file micro-ROS_vIO_topic.ino
 * @brief micro-ROS Publisher and Subscriber Example
 * 
 * This example demonstrates pub/sub communication with digital I/O:
 * - Publisher: Publishes digital input states via /DigitalInput/read topic
 * - Subscriber: Receives digital output commands via /DigitalOutput/write topic
 * It's based on [Virtual Arduino](https://www.qec.tw/ethercat/86eva/).
 * 
 * Network Configuration:
 *   - QEC IP:   192.168.3.202
 *   - Agent IP: 192.168.3.59
 *   - Port:     9999
 * 
 * Test Commands:
 * - Digital input read:
``` Bash
ros2 topic echo /DigitalInput/read
```
   
 * - Digital output write:
``` Bash
ros2 topic pub --once /DigitalOutput/write std_msgs/msg/UInt8MultiArray \
  "{data: [0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1]}"
```
   
 * - Digital output write (continuous loop):
``` Bash
while true; do
  ros2 topic pub --once /DigitalOutput/write std_msgs/msg/UInt8MultiArray \
    "{data: [0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1]}"
  sleep 1
  ros2 topic pub --once /DigitalOutput/write std_msgs/msg/UInt8MultiArray \
    "{data: [1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0]}"
  sleep 1
done 
```
 */

#include <micro_ros_arduino.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/u_int8_multi_array.h>

#include "myeva.h"

// ============================================================================
// Global ROS 2 Variables
// ============================================================================
rcl_node_t node;
rclc_support_t support;
rcl_allocator_t allocator;
rclc_executor_t executor;
rcl_ret_t rcl_ret;

// ============================================================================
// Publisher Variables
// ============================================================================
rcl_publisher_t publisher;
std_msgs__msg__UInt8MultiArray digitalInputMessage;
rcl_timer_t timer;

// ============================================================================
// Subscriber Variables
// ============================================================================
rcl_subscription_t subscriber;
std_msgs__msg__UInt8MultiArray digitalOutputMessage;


// ============================================================================
// Callbacks
// ============================================================================

// Forward declarations
void onInputPublisherTimer(rcl_timer_t * timer, int64_t lastCallTime);
void onDigitalOutputReceived(const void * message);

/**
 * @brief Timer callback for publishing digital input states
 * 
 * Periodically reads digital inputs (pins 16-31) and publishes changes
 * only when values differ from last reading (change-based publishing).
 * 
 * @param timer Pointer to the timer object
 * @param lastCallTime Timestamp of last timer call
 */
void onInputPublisherTimer(rcl_timer_t * timer, int64_t lastCallTime) {
  int i;
  bool hasInputChanged = false;
  
  if (timer == NULL) return;
  
  // Check each input and flag for publish if changed
  for (i = 0; i < 16; ++i) {
    if (digitalInputMessage.data.data[i] != EVA.digitalRead(16 + i)) {
      digitalInputMessage.data.data[i] = EVA.digitalRead(16 + i);
      hasInputChanged = true;
    }
  }
  
  // Publish only if changes detected
  if (hasInputChanged) {
    rcl_publish(&publisher, &digitalInputMessage, NULL);
  }
}

/**
 * @brief Subscription callback for digital output commands
 * 
 * Called when a new UInt8MultiArray message is received on /DigitalOutput/write.
 * Sets the digital output pins (0-15) according to the received values.
 * 
 * @param message Pointer to the received message
 */
void onDigitalOutputReceived(const void * message) {
  int i;
  
  // Set each digital output pin
  for (i = 0; i < 16; ++i) {
    EVA.digitalWrite(i, digitalOutputMessage.data.data[i]);
  }
}


// ============================================================================
// Setup and Initialization
// ============================================================================

/**
 * @brief Initialize micro-ROS publisher and subscriber
 * 
 * Publishes to:  /DigitalInput/read (UInt8MultiArray)
 * Subscribes to: /DigitalOutput/write (UInt8MultiArray)
 */
void setup() {
  Serial.begin(3000000);
  int i;
  
  // Ethernet MAC address
  byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
  
  // Configure micro-ROS UDP transport
  IPAddress qec_ip(192, 168, 3, 202);
  IPAddress agent_ip(192, 168, 3, 59);
  set_microros_native_ethernet_udp_transports(mac, qec_ip, agent_ip, 9999);

  delay(2000);
  allocator = rcl_get_default_allocator();

  // Initialize ROS 2 support with automatic reconnection on failure
  do {
    rcl_ret = rclc_support_init(&support, 0, NULL, &allocator);
    if (rcl_ret != RCL_RET_OK) {
      Serial.println("ROS2 support init failed.");
      Serial.println("Error code: %d", rcl_ret);
    } else {
      Serial.println("ROS2 support initialized successfully.");
    }
  } while (rcl_ret != RCL_RET_OK);

  // Create ROS 2 node
  rclc_node_init_default(&node, "qec_node", "", &support);

  // Allocate memory for input message (16 bytes for digital input pins)
  digitalInputMessage.data.capacity = 16;
  digitalInputMessage.data.size = 16;
  digitalInputMessage.data.data = (uint8_t *)malloc(sizeof(uint8_t) * 16);
  if (!digitalInputMessage.data.data) {
    Serial.println("Failed to allocate memory for input message.");
    while (1); // Stop if allocation fails
  }

  // Create publisher for input values
  rclc_publisher_init_best_effort(
    &publisher, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, UInt8MultiArray),
    "/DigitalInput/read"
  );

  // Create timer for periodic input publishing (100ms interval)
  rclc_timer_init_default(&timer, &support, RCL_MS_TO_NS(100), onInputPublisherTimer);

  // Allocate memory for output message (16 bytes for digital output pins)
  digitalOutputMessage.data.capacity = 16;
  digitalOutputMessage.data.size = 16;
  digitalOutputMessage.data.data = (uint8_t *)malloc(sizeof(uint8_t) * 16);
  if (!digitalOutputMessage.data.data) {
    Serial.println("Failed to allocate memory for output message");
    while (1); // Stop if allocation fails
  }

  // Create subscriber for output commands
  rclc_subscription_init_best_effort(
    &subscriber,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, UInt8MultiArray),
    "/DigitalOutput/write"
  );

  // Initialize executor with 2 handles (timer + subscription)
  rclc_executor_init(&executor, &support.context, 2, &allocator);

  // Add components to executor
  rclc_executor_add_timer(&executor, &timer);
  rclc_executor_add_subscription(&executor, &subscriber, &digitalOutputMessage, 
                                 &onDigitalOutputReceived, ON_NEW_DATA);

  // Initialize EVA
  EVA.begin();

  // Initialize all digital outputs to HIGH
  for (i = 0; i < 16; ++i) {    
    digitalOutputMessage.data.data[i] = 1;
    EVA.digitalWrite(i, digitalOutputMessage.data.data[i]);
  }
}

/**
 * @brief Spin the ROS 2 executor
 */
void loop() {
  rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));
}
