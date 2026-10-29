#pragma once

#include <algorithm>
#include <array>
#include <deque>
#include <functional>
#include <unordered_set>
#include <vector>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace proflame2 {

enum class Proflame2Function : uint8_t {
  AUX = 0,
  COMFORT_FAN,
  CONTROL,
  FLAME,
  LIGHT,
  MODE,
  PILOT_LIGHT,
  SPLIT_FLOW,
};

enum class Proflame2CommandType : uint8_t {
  SET = 0x5D,
  GET = 0x5E,
  SET_RESPONSE = 0xAD,
  GET_RESPONSE = 0xAE,
};

enum class Proflame2Mode : uint8_t {
  OFF = 0,
  ON = 1,
  THERMOSTAT = 2,
  SMART = 3,
};

enum class Proflame2Register : uint8_t {
  CONTROL = 0x16,
  STATE_1 = 0x17,
  STATE_2 = 0x18,
};

struct Proflame2PollRequest {
  bool CONTROL : 1 = false;
  bool STATE_1 : 1 = false;
  bool STATE_2 : 1 = false;
};

struct Proflame2Command {
  Proflame2CommandType type;
  std::array<uint8_t, 3> payload;
  Proflame2Register reg;
};

struct Proflame2Packet {
  Proflame2Command cmd;
  uint8_t checksum;
};

struct Proflame2QueueCommand {
  Proflame2Command command;  // The command to send.
  uint32_t timestamp = 0;    // Timestamp when the command was added to the queue.
};

enum class Proflame2ControlState : uint8_t {
  UNKNOWN = 0x00,  // Control state is unknown.
  UART,            // UART client has control.
  EXTERNAL,        // Other remote has control.
};

// Internal state of the Proflame2 IFC.

struct Proflame2State1 {
  Proflame2Mode mode : 4 = Proflame2Mode::OFF;  // Current mode of the fireplace (needs to be at least 'ON' for the
                                                // fireplace to accept some commands)

  uint8_t light_level : 3 = 0;           // Light brightness level (0=off, 1–6).
  bool continuous_pilot_on : 1 = false;  // Whether the continuous pilot light is enabled.
                                         // By default intermittent pilot mode is used.
  auto operator<=>(const Proflame2State1 &) const = default;
};

struct Proflame2State2 {
  uint8_t flame_height : 3 = 0;       // Flame height level (0=off, 1–6).
  bool aux_on : 1 = false;            // Whether the auxiliary relay is on.
  uint8_t comfort_fan_speed : 3 = 0;  // Comfort fan speed level (0=off, 1–6).
  bool split_flow_on : 1 = false;     // Whether secondary burner is on.
  auto operator<=>(const Proflame2State2 &) const = default;
};

struct Proflame2State {
  struct Proflame2State1 state1;
  struct Proflame2State2 state2;
  Proflame2ControlState control =
      Proflame2ControlState::UNKNOWN;  // Whether UART is currently in control of the fireplace.
  auto operator<=>(const Proflame2State &) const = default;
};

struct Proflame2FunctionListener {
  Proflame2Function function;  // The function this listener is interested in.
  std::function<void(Proflame2State *)>
      on_state_change;  // Callback to send the current state when the listener is triggered.
};

// register custom to_string functions for logging and debugging
using std::to_string;
std::string to_string(const Proflame2Function &function);
std::string to_string(const Proflame2CommandType &type);
std::string to_string(const Proflame2Register &reg);
std::string to_string(const Proflame2Mode &mode);
std::string to_string(const Proflame2Packet &packet);
std::string to_string(const Proflame2ControlState &state);
std::string to_string(const Proflame2State &state);

class Proflame2 : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;

  void dump_state();

  void register_listener(Proflame2Function function, const std::function<void(Proflame2State *)> &on_state_change);
  static uint8_t calculate_checksum(Proflame2Packet *packet);
  static bool validate_checksum(Proflame2Packet *packet);

  // Setters and getters for the Proflame2 state.
  void set_light_brightness(uint8_t brightness) {
    this->ensure_on();
    this->state_.state1.light_level = std::clamp(brightness, uint8_t{0}, uint8_t{6});
    this->send_state_();
  }
  uint8_t get_light_brightness() const { return this->state_.state1.light_level; }
  void enable_continuous_pilot(bool on) {
    this->state_.state1.continuous_pilot_on = on;
    this->send_state_();
  };
  bool get_pilot_light() const { return this->state_.state1.continuous_pilot_on; }
  void set_aux_on(bool on) {
    this->ensure_on();
    this->state_.state2.aux_on = on;
    this->send_state_();
  };
  bool get_aux_is_on() const { return this->state_.state2.aux_on; }
  void set_comfort_fan_speed(uint8_t speed) {
    this->ensure_on();
    this->state_.state2.comfort_fan_speed = std::clamp(speed, uint8_t{0}, uint8_t{6});
    this->send_state_();
  }
  uint8_t get_comfort_fan_speed() const { return this->state_.state2.comfort_fan_speed; }
  void set_flame_height(uint8_t height) {
    this->ensure_on();
    this->state_.state2.flame_height = std::clamp(height, uint8_t{0}, uint8_t{6});
    this->send_state_();
  }
  uint8_t get_flame_height() const { return this->state_.state2.flame_height; }
  void enable_split_flow(bool on) {
    this->ensure_on();
    this->state_.state2.split_flow_on = on;
    this->send_state_();
  };
  bool get_split_flow_is_on() const { return this->state_.state2.split_flow_on; }

  bool get_switch_is_on(Proflame2Function function);
  void set_switch(Proflame2Function function, bool on);

  Proflame2Mode get_mode() const { return this->state_.state1.mode; }
  void ensure_on() { this->state_.state1.mode = std::max(this->state_.state1.mode, Proflame2Mode::ON); }
  bool is_on() const { return this->state_.state1.mode >= Proflame2Mode::ON; }

#ifdef USE_BINARY_SENSOR
  void set_control_binary_sensor(binary_sensor::BinarySensor *control_binary_sensor) {
    this->control_binary_sensor_ = control_binary_sensor;
  }
#endif

 protected:
  Proflame2State state_;
  Proflame2State last_reported_state_;
  std::vector<Proflame2FunctionListener> listeners_;
  std::unordered_set<Proflame2Function> changed_functions_{};

  std::deque<Proflame2QueueCommand> command_queue_;
  uint32_t last_command_timestamp_ = 0;
  Proflame2PollRequest poll_registers_ = {};

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *control_binary_sensor_{nullptr};
#endif

  void send_raw_command_(Proflame2Command command);
  void process_command_queue_();
  void send_command_(const Proflame2Command &command);
  void send_state_();
  void poll_state_(Proflame2Register reg, bool soon = false);
  void poll_state_(bool soon = false);
  void assert_control_();
  void update_control_sensor_();

  void flush_listeners_();
  void flush_all_listeners_();
  void received_packet_(const Proflame2Packet *packet);
  template<typename T> bool update_if_changed_(Proflame2Function func, T new_value);
  void handle_control_response_(const Proflame2Packet *packet);
  void update_state1_(Proflame2State1 state);
  void update_state2_(Proflame2State2 state);
};
}  // namespace proflame2
}  // namespace esphome
