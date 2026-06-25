/**
 * @file micro-ROS_vServo_topic.ino
 * @brief micro-ROS Publisher and Subscriber Example - Virtual Servo/Encoder
 * 
 * This example demonstrates pub/sub communication with virtual servo motors and encoders :
 * - Publisher: Publishes encoder readings via /VirtualEncoder/read topic
 * - Subscriber: Receives servo position commands via /VirtualServo/write topic
 * It's based on [Virtual Arduino](https://www.qec.tw/ethercat/86eva/).
 * 
 * Network Configuration:
 *   - QEC IP:   192.168.3.202
 *   - Agent IP: 192.168.3.59
 *   - Port:     9999
 * 
 * Test Commands:
 * - Read virtual encoder:
``` Bash
ros2 topic echo /VirtualEncoder/read
```
   
 * - Set all servos to 0 degrees:
``` Bash
ros2 topic pub --once /VirtualServo/write std_msgs/msg/Int32MultiArray \
  "{data: [0, 0, 0]}"
```
   
 * - Set all servos to 360 degrees:
``` Bash
ros2 topic pub --once /VirtualServo/write std_msgs/msg/Int32MultiArray \
  "{data: [360, 360, 360]}"
```
   
 * - Set all servos to 0 and 360 degrees (continuous loop):
``` Bash
while true; do
  ros2 topic pub --once /VirtualServo/write std_msgs/msg/Int32MultiArray \
    "{data: [0, 0, 0]}"
  sleep 2
  ros2 topic pub --once /VirtualServo/write std_msgs/msg/Int32MultiArray \
    "{data: [360, 360, 360]}"
  sleep 2
done
```
 */

#include <micro_ros_arduino.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/u_int32_multi_array.h>
#include <std_msgs/msg/int32_multi_array.h>

#include "myeva.h"

// ============================================================================
// Configuration Constants
// ============================================================================
#define VSERVO_SIZE   3    // Number of virtual servos
#define VENCODER_SIZE 3    // Number of virtual encoders

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
std_msgs__msg__UInt32MultiArray encoderReadingMessage;
rcl_timer_t timer;

// ============================================================================
// Subscriber Variables
// ============================================================================
rcl_subscription_t subscriber;
std_msgs__msg__Int32MultiArray servoCommandMessage;

// ============================================================================
// Hardware Abstraction Layer References
// ============================================================================
MyVirtualServo* VirtualServo[] = {&VirtualServo1, &VirtualServo2, &VirtualServo3};
MyVirtualEncoder* VirtualEncoder[] = {&VirtualEncoder1, &VirtualEncoder2, &VirtualEncoder3};

// ============================================================================
// Callbacks
// ============================================================================

// Forward declarations
void onEncoderPublisherTimer(rcl_timer_t * timer, int64_t lastCallTime);
void onServoCommandReceived(const void * message);

/**
 * @brief Timer callback for publishing encoder values
 * 
 * Periodically reads all encoder values and publishes changes
 * only when values differ from last reading (change-based publishing).
 * 
 * @param timer Pointer to the timer object
 * @param lastCallTime Timestamp of last timer call
 */
void onEncoderPublisherTimer(rcl_timer_t * timer, int64_t lastCallTime) {
  int i;
  bool hasEncoderChanged = false;
  
  if (timer == NULL) return;
  
  // Check each encoder and flag for publish if changed
  for (i = 0; i < VENCODER_SIZE; ++i) {
    if (encoderReadingMessage.data.data[i] != VirtualEncoder[i]->read()) {
      encoderReadingMessage.data.data[i] = VirtualEncoder[i]->read();
      hasEncoderChanged = true;
    }
  }
  
  // Publish only if changes detected
  if (hasEncoderChanged) {
    rcl_publish(&publisher, &encoderReadingMessage, NULL);
  }
}

/**
 * @brief Subscription callback for servo position commands
 * 
 * Called when a new Int32MultiArray message is received on /VirtualServo/write.
 * Sets the position of each virtual servo according to received values.
 * 
 * @param message Pointer to the received message
 */
void onServoCommandReceived(const void * message) {
  int i;
  
  // Set position for each servo
  for (i = 0; i < VSERVO_SIZE; ++i) {
    VirtualServo[i]->write(servoCommandMessage.data.data[i]);
  }
}


// ============================================================================
// Setup and Initialization
// ============================================================================

/**
 * @brief Initialize micro-ROS publisher and subscriber
 * 
 * Publishes to:  /VirtualEncoder/read (UInt32MultiArray)
 * Subscribes to: /VirtualServo/write (Int32MultiArray)
 */
void setup() {
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
      printf("ROS2 support init failed\n");
      printf("Error code: %d\n", rcl_ret);
    } else {
      printf("ROS2 support initialized successfully\n");
    }
  } while (rcl_ret != RCL_RET_OK);

  // Create ROS 2 node
  rclc_node_init_default(&node, "qec_node", "", &support);

  // Allocate memory for encoder message
  encoderReadingMessage.data.capacity = VENCODER_SIZE;
  encoderReadingMessage.data.size = VENCODER_SIZE;
  encoderReadingMessage.data.data = (uint32_t *)malloc(sizeof(uint32_t) * VENCODER_SIZE);
  if (!encoderReadingMessage.data.data) {
    printf("Failed to allocate memory for encoder message\n");
    while (1); // Stop if allocation fails
  }

  // Create publisher for encoder values
  rclc_publisher_init_best_effort(
    &publisher, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, UInt32MultiArray),
    "/VirtualEncoder/read"
  );

  // Create timer for periodic encoder publishing (100ms interval)
  rclc_timer_init_default(&timer, &support, RCL_MS_TO_NS(100), onEncoderPublisherTimer);

  // Allocate memory for servo message
  servoCommandMessage.data.capacity = VSERVO_SIZE;
  servoCommandMessage.data.size = VSERVO_SIZE;
  servoCommandMessage.data.data = (int32_t *)malloc(sizeof(int32_t) * VSERVO_SIZE);
  if (!servoCommandMessage.data.data) {
    printf("Failed to allocate memory for servo message\n");
    while (1); // Stop if allocation fails
  }

  // Create subscriber for servo commands
  rclc_subscription_init_best_effort(
    &subscriber,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32MultiArray),
    "/VirtualServo/write"
  );

  // Initialize executor with 2 handles (timer + subscription)
  rclc_executor_init(&executor, &support.context, 2, &allocator);

  // Add components to executor
  rclc_executor_add_timer(&executor, &timer);
  rclc_executor_add_subscription(&executor, &subscriber, &servoCommandMessage, 
                                 &onServoCommandReceived, ON_NEW_DATA);

  // Initialize EVA
  EVA.begin();

  // ========================================================================
  // Initialize all virtual encoders
  // ========================================================================
  for (i = 0; i < VENCODER_SIZE; ++i) {
    VirtualEncoder[i]->setMode(ECAT_ENCODER_MODE_STEP_DIR);
    VirtualEncoder[i]->write(0); // Reset to 0
  }

  // ========================================================================
  // Initialize all virtual servos
  // ========================================================================
  for (i = 0; i < VSERVO_SIZE; ++i) {
    servoCommandMessage.data.data[i] = 0; // Initialize command to 0 degrees
    VirtualServo[i]->setVelocity(3200.0);      // Set velocity
    VirtualServo[i]->setAcceleration(3200.0);  // Set acceleration
    VirtualServo[i]->setPositionType(VIRTUALSERVO_ABSOLUTE_POSITION_ANGLE);
    VirtualServo[i]->write(servoCommandMessage.data.data[i]); // Move to initial position
  }
}

/**
 * @brief Spin the ROS 2 executor
 */
void loop() {
  rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));
}