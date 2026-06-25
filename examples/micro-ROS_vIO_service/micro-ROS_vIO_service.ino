/**
 * @file micro-ROS_vIO_service.ino
 * @brief micro-ROS Service Server and Client Example
 * 
 * This example demonstrates bidirectional service communication:
 * - Server: Provides a SetBool service that controls digital outputs
 * - Client: Calls a remote SetBool service based on button input
 * It's based on [Virtual Arduino](https://www.qec.tw/ethercat/86eva/).
 * 
 * Network Configuration:
 *   - QEC IP:   192.168.3.202
 *   - Agent IP: 192.168.3.59
 *   - Port:     9999
 * 
 * Test Commands:
 * - Digital input read (with python):
``` Bash
python3 -c "
import rclpy
from std_srvs.srv import SetBool

def callback(request, response):
    response.success = True
    response.message = f'Received: {request.data}'
    print(f'micro-ROS sent: {request.data}')
    return response

rclpy.init()
node = rclpy.create_node('pc_server')
srv = node.create_service(SetBool, '/DigitalInput/read', callback)
print('Service /DigitalInput/read ready...')
rclpy.spin(node)
"
```
   
 * - Digital output write - true:
``` Bash
ros2 service call /DigitalOutput/write std_srvs/srv/SetBool "{data: true}"
```
   
 * - Digital output write (continuous loop):
``` Bash
while true; do
  ros2 service call /DigitalOutput/write std_srvs/srv/SetBool "{data: true}"
  sleep 1
  ros2 service call /DigitalOutput/write std_srvs/srv/SetBool "{data: false}"
  sleep 1
done
```
 */

#include <micro_ros_arduino.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/u_int8_multi_array.h>
#include <std_srvs/srv/set_bool.h>

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
// Service Server Variables
// ============================================================================
rcl_service_t service;
std_srvs__srv__SetBool_Request ledServiceRequest;
std_srvs__srv__SetBool_Response ledServiceResponse;

// ============================================================================
// Service Client Variables
// ============================================================================
rcl_timer_t timer;
rcl_client_t client;
std_srvs__srv__SetBool_Request clientRequest;
std_srvs__srv__SetBool_Response clientResponse;

// ============================================================================
// Service Callbacks
// ============================================================================

// Forward declarations
void onLedServiceRequest(const void * request, void * response);
void onClientServiceResponse(const void * response);
void onServiceCheckTimer(rcl_timer_t * timer, int64_t lastCallTime);

/**
 * @brief Service callback for /DigitalOutput/write
 * 
 * Handles incoming SetBool requests and controls LED outputs:
 * - If request.data is true: Set even-indexed LEDs ON
 * - If request.data is false: Set odd-indexed LEDs ON
 * 
 * @param request Pointer to request structure
 * @param response Pointer to response structure
 */
void onLedServiceRequest(const void * request, void * response) {
  int i;
  std_srvs__srv__SetBool_Request * req = (std_srvs__srv__SetBool_Request *) request;
  std_srvs__srv__SetBool_Response * res = (std_srvs__srv__SetBool_Response *) response;

  if (req->data) {
    // Set even-indexed LEDs ON
    for (i = 0; i < 16; ++i) {
      EVA.digitalWrite(i, i % 2 == 0);
    }
    res->success = true;
    res->message.data = (char*)">> true";
    res->message.size = strlen(res->message.data);
    Serial.println("Service: LED pattern set to EVEN");
  } else {
    // Set odd-indexed LEDs ON
    for (i = 0; i < 16; ++i) {
      EVA.digitalWrite(i, i % 2 == 1);
    }
    res->success = false;
    res->message.data = (char*)">> false";
    res->message.size = strlen(res->message.data);
    Serial.println("Service: LED pattern set to ODD");
  }
}

/**
 * @brief Client response callback for /DigitalInput/read service
 * 
 * Processes the response from the remote service call.
 * 
 * @param response Pointer to the response structure
 */
void onClientServiceResponse(const void * response) {
  std_srvs__srv__SetBool_Response * res = (std_srvs__srv__SetBool_Response *) response;

  Serial.print("Client response - Success: ");
  Serial.print(res->success);
  Serial.print(" | Message: ");
  Serial.println(res->message.data);
}

/**
 * @brief Timer callback for periodic client service calls
 * 
 * Monitors digital inputs (pins 16 and 17) and calls the remote service:
 * - Pin 16 HIGH: Send SetBool(true)
 * - Pin 17 HIGH: Send SetBool(false)
 * 
 * @param timer Pointer to the timer object
 * @param lastCallTime Timestamp of last timer call
 */
void onServiceCheckTimer(rcl_timer_t * timer, int64_t lastCallTime) {
  int64_t requestSequenceId;
  if (timer == NULL) return;

  // Check input states and send client request accordingly
  if (!clientRequest.data && EVA.digitalRead(16)) {
    clientRequest.data = true;
    rcl_send_request(&client, &clientRequest, &requestSequenceId);
    Serial.println("Client: Sending SetBool(true)...");
  } else if (clientRequest.data && EVA.digitalRead(17)) {
    clientRequest.data = false;
    rcl_send_request(&client, &clientRequest, &requestSequenceId);
    Serial.println("Client: Sending SetBool(false)...");
  }
}


// ============================================================================
// Setup and Initialization
// ============================================================================

/**
 * @brief Initialize micro-ROS service server and client
 * 
 * Provides service:  /DigitalOutput/write (SetBool)
 * Calls service:     /DigitalInput/read (SetBool)
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

  // Create timer for periodic client calls (100ms interval)
  rclc_timer_init_default(&timer, &support, RCL_MS_TO_NS(100), onServiceCheckTimer);

  // Create service server
  rclc_service_init_default(
    &service, &node,
    ROSIDL_GET_SRV_TYPE_SUPPORT(std_srvs, srv, SetBool),
    "/DigitalOutput/write"
  );

  // Create service client
  rclc_client_init_default(
    &client, &node,
    ROSIDL_GET_SRV_TYPE_SUPPORT(std_srvs, srv, SetBool),
    "/DigitalInput/read"
  );

  // Allocate buffer for client response message
  clientResponse.message.data = (char*)malloc(sizeof(char) * 100);
  clientResponse.message.capacity = 100;
  clientResponse.message.size = 0;

  // Initialize executor with 3 handles (timer, service server, service client)
  rclc_executor_init(&executor, &support.context, 3, &allocator);

  // Add components to executor
  rclc_executor_add_timer(&executor, &timer);
  rclc_executor_add_service(&executor, &service, &ledServiceRequest, 
                            &ledServiceResponse, onLedServiceRequest);
  rclc_executor_add_client(&executor, &client, &clientResponse, 
                           onClientServiceResponse);

  // Initialize EVA
  EVA.begin();

  // Initialize all digital outputs to HIGH
  for (i = 0; i < 16; ++i) {
    EVA.digitalWrite(i, 1);
  }
}

/**
 * @brief Spin the ROS 2 executor
 */
void loop() {
  rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));
}
