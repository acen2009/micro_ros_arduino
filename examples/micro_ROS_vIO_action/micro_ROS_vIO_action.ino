/**
 * @file micro_ROS_vIO_action.ino
 * @brief micro-ROS Action Server Example
 * 
 * This example demonstrates how to implement an Action Server using micro-ROS.
 * The server executes a Fibonacci-like action that cycles through 16 digital outputs
 * over time, sending feedback at each step and returning a result when complete.
 * It's based on [Virtual Arduino](https://www.qec.tw/ethercat/86eva/).
 * 
 * Network Configuration:
 *   - QEC IP:   192.168.3.202
 *   - Agent IP: 192.168.3.59
 *   - Port:     9999
 * 
 * Test Commands:
 * - Play digital output action (step delay 100ms):
``` Bash
ros2 action send_goal /DigitalOutput/action example_interfaces/action/Fibonacci "{order: 100}" --feedback
```
 */

#include <micro_ros_arduino.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <example_interfaces/action/fibonacci.h>

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
// Action Server Variables
// ============================================================================
rclc_action_server_t action_server;
example_interfaces__action__Fibonacci_FeedbackMessage actionFeedback;
example_interfaces__action__Fibonacci_SendGoal_Request actionGoalRequest[1];
rcl_timer_t timer;

// Action execution state
bool isActionRunning = false;
int currentStep = 0;
int goalStepDelayMs = 0;
rclc_action_goal_handle_t * currentGoalHandle;

// ============================================================================
// Action Callbacks
// ============================================================================

// Forward declarations
rcl_ret_t onActionGoalReceived(rclc_action_goal_handle_t * goalHandle, void * context);
bool onActionCancelRequested(rclc_action_goal_handle_t * goalHandle, void * context);
void onActionExecutionTimer(rcl_timer_t * timer, int64_t lastCallTime);

/**
 * @brief Handle incoming goal requests
 * 
 * Called when a client sends a goal to this action server.
 * Initializes the action execution state and returns ACCEPTED.
 * 
 * @param goalHandle Pointer to the goal handle
 * @param context User-provided context (unused)
 * @return RCL_RET_ACTION_GOAL_ACCEPTED if goal is accepted
 */
rcl_ret_t onActionGoalReceived(rclc_action_goal_handle_t * goalHandle, void * context) {
  example_interfaces__action__Fibonacci_SendGoal_Request * request =
    (example_interfaces__action__Fibonacci_SendGoal_Request *) goalHandle->ros_goal_request;

  // Extract goal value (step delay in milliseconds)
  goalStepDelayMs = request->goal.order;
  currentStep = 0;
  currentGoalHandle = goalHandle;
  isActionRunning = true;

  Serial.println("Goal Accepted, starting task...");
  return RCL_RET_ACTION_GOAL_ACCEPTED;
}

/**
 * @brief Handle goal cancellation requests
 * 
 * Called when a client sends a cancel request to this action.
 * Stops the current action execution.
 * 
 * @param goalHandle Pointer to the goal handle
 * @param context User-provided context (unused)
 * @return true to accept cancellation
 */
bool onActionCancelRequested(rclc_action_goal_handle_t * goalHandle, void * context) {
  Serial.println("Goal canceled...");
  isActionRunning = false;
  return true;
}

/**
 * @brief Timer callback for action execution and feedback publishing
 * 
 * Called periodically to:
 * - Check action running status
 * - Publish feedback at each step
 * - Control LED output
 * - Send result when action completes
 * 
 * @param timer Pointer to the timer object
 * @param lastCallTime Timestamp of last timer call
 */
void onActionExecutionTimer(rcl_timer_t * timer, int64_t lastCallTime) {
  int i;
  static unsigned long lastStepTime = 0;
  
  if (timer == NULL) return;
  if (!isActionRunning) return;

  // Delay between steps based on goal step delay (in milliseconds)
  if (millis() - lastStepTime < goalStepDelayMs) return;
  lastStepTime = millis();

  // Publish feedback with current step
  int32_t stepFeedbackValue = currentStep;
  actionFeedback.feedback.sequence.data = &stepFeedbackValue;
  actionFeedback.feedback.sequence.size = 1;
  actionFeedback.feedback.sequence.capacity = 1;
  rclc_action_publish_feedback(currentGoalHandle, &actionFeedback);

  Serial.print("Step: ");
  Serial.println(currentStep);

  // Set one LED ON at currentStep position, others OFF
  for (i = 0; i < 16; ++i) {
    EVA.digitalWrite(i, i == currentStep);
  }

  // Check if action is complete (16 steps)
  if (currentStep >= 16) {
    example_interfaces__action__Fibonacci_GetResult_Response actionResult = {0};
    rclc_action_send_result(currentGoalHandle, GOAL_STATE_SUCCEEDED, &actionResult);
    isActionRunning = false;
    Serial.println("Action Finished!");
  }
  currentStep++;
}


// ============================================================================
// Setup and Initialization
// ============================================================================

/**
 * @brief Initialize micro-ROS and action server
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
      printf("ROS2 support init failed\n");
      printf("Error code: %d\n", rcl_ret);
    } else {
      printf("ROS2 support initialized successfully\n");
    }
  } while (rcl_ret != RCL_RET_OK);

  // Create ROS 2 node
  rclc_node_init_default(&node, "qec_node", "", &support);

  // Create timer for action execution (10ms interval)
  rclc_timer_init_default(&timer, &support, RCL_MS_TO_NS(10), onActionExecutionTimer);

  // Create action server
  rclc_action_server_init_default(
    &action_server,
    &node,
    &support,
    ROSIDL_GET_ACTION_TYPE_SUPPORT(example_interfaces, Fibonacci),
    "/DigitalOutput/action"
  );

  // Initialize executor with sufficient handles for action server
  rclc_executor_init(&executor, &support.context, 10, &allocator);

  // Add timer and action server to executor
  rclc_executor_add_timer(&executor, &timer);
  rclc_executor_add_action_server(
    &executor,
    &action_server,
    1,
    actionGoalRequest,
    sizeof(example_interfaces__action__Fibonacci_SendGoal_Request),
    onActionGoalReceived,
    onActionCancelRequested,
    (void *) &action_server
  );

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
