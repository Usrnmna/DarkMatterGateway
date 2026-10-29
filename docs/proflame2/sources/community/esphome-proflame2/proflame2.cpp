#include <bit>
#include <cstdint>
#include <format>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include "proflame2.h"

namespace esphome {
namespace proflame2 {

static const char *const TAG = "proflame2";
// Log packet related messages to different tag to ease filtering during debugging
static const char *const PACKET_TAG = "proflame2.packet";

constexpr int COMMAND_DELAY = 50;     // milliseconds
constexpr int RECEIVE_TIMEOUT = 250;  // milliseconds

// Magic packets
// These packets are sent as part of the 0x16 (control) register
// As we don't know what the bits mean we use them verbatim
constexpr std::array<uint8_t, 3> PKT_CTRL_EXTERNAL{0x00, 0x00, 0x00};  // External control mode
constexpr std::array<uint8_t, 3> PKT_CTRL_UART{0xc1, 0xa0, 0x00};      // UART control mode

// Response packet indicator
// Response packet headers are request headers XORed with 0xF0
// i.e. a 0x5D request packet becomes a corresponding 0xAD response packet
constexpr uint8_t RESPONSE_MASK = 0xa0;

// Bitmask for STATE1 features that require the power bit to be set
// (i.e. everything but the pilot light setting)
constexpr uint8_t POWER_ON_ONLY_FEATURES_MASK = 0x7c;

// Bitmask for the Continuous Pilot Light setting
constexpr uint8_t CONTINUOUS_PILOT_BIT = 0x07;

void Proflame2::setup() { this->assert_control_(); }

void Proflame2::update() {
  this->poll_state_(Proflame2Register::CONTROL);

  if (!(this->state_.control == Proflame2ControlState::UART)) {
    this->assert_control_();
  } else {
    this->poll_state_();
  }
}

void Proflame2::loop() {
  if (this->available()) {
    auto data = this->read_array<6>();
    if (data) {
      auto packet = std::bit_cast<Proflame2Packet>(*data);
      bool is_valid = validate_checksum(&packet);
      if (is_valid) {
        ESP_LOGVV(PACKET_TAG, to_string(packet).c_str());
        this->received_packet_(&packet);
      }
    }
  }

  this->process_command_queue_();
}

void Proflame2::dump_config() {
  ESP_LOGCONFIG(TAG, "Proflame 2:");
  LOG_UPDATE_INTERVAL(this);
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "In control", this->control_binary_sensor_);
#endif

  this->check_uart_settings(9600);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

void Proflame2::dump_state() { ESP_LOGI(TAG, to_string(this->state_).c_str()); }

void Proflame2::register_listener(Proflame2Function function,
                                  const std::function<void(Proflame2State *)> &on_state_change) {
  auto listener = Proflame2FunctionListener{
      .function = function,
      .on_state_change = on_state_change,
  };
  this->listeners_.push_back(listener);

  on_state_change(&this->state_);  // Send the current state to the listener immediately
}

// String conversion helper functions

std::string to_string(const Proflame2Function &function) {
  switch (function) {
    case Proflame2Function::AUX:
      return "AUX";
    case Proflame2Function::COMFORT_FAN:
      return "COMFORT_FAN";
    case Proflame2Function::CONTROL:
      return "CONTROL";
    case Proflame2Function::FLAME:
      return "FLAME";
    case Proflame2Function::LIGHT:
      return "LIGHT";
    case Proflame2Function::MODE:
      return "MODE";
    case Proflame2Function::PILOT_LIGHT:
      return "PILOT_LIGHT";
    case Proflame2Function::SPLIT_FLOW:
      return "SPLIT_FLOW";
    default:
      return "UNKNOWN";
  }
}

std::string to_string(const Proflame2CommandType &type) {
  auto t = static_cast<uint8_t>(type);
  return std::format("{} {}", (t & 0xf) == 0xd ? "SET" : "GET", t & RESPONSE_MASK ? "RESPONSE" : "REQUEST");
}

std::string to_string(const Proflame2Register &reg) {
  switch (reg) {
    case Proflame2Register::CONTROL:
      return "CONTROL";
    case Proflame2Register::STATE_1:
      return "STATE_1";
    case Proflame2Register::STATE_2:
      return "STATE_2";
    default:
      return "UNKNOWN";
  }
}

std::string to_string(const Proflame2Mode &mode) {
  switch (mode) {
    case Proflame2Mode::OFF:
      return "OFF";
    case Proflame2Mode::ON:
      return "ON";
    case Proflame2Mode::THERMOSTAT:
      return "THERMOSTAT";
    case Proflame2Mode::SMART:
      return "SMART";
    default:
      return "UNKNOWN";
  }
}

std::string to_string(const Proflame2Packet &packet) {
  return std::format("Proflame2Packet{{cmd={}, payload=[{:#04x}, {:#04x}, {:#04x}], reg={}, checksum={:#04x}}}",
                     to_string(packet.cmd.type), packet.cmd.payload[0], packet.cmd.payload[1], packet.cmd.payload[2],
                     to_string(packet.cmd.reg), packet.checksum);
}

std::string to_string(const Proflame2ControlState &state) {
  switch (state) {
    case Proflame2ControlState::UNKNOWN:
      return "Unknown";
    case Proflame2ControlState::UART:
      return "UART";
    case Proflame2ControlState::EXTERNAL:
      return "External";
    default:
      return "Invalid";
  }
}

std::string to_string(const Proflame2State &state) {
  return "Proflame2State{control=" + to_string(state.control) +
         ", pilot_on=" + to_string(state.state1.continuous_pilot_on) +
         ", light_level=" + to_string(state.state1.light_level) + ", mode=" + to_string(state.state1.mode) +
         ", flame_height=" + to_string(state.state2.flame_height) + ", aux_on=" + TRUEFALSE(state.state2.aux_on) +
         ", comfort_fan_speed=" + to_string(state.state2.comfort_fan_speed) +
         ", split_flow_on=" + TRUEFALSE(state.state2.split_flow_on) + "}";
}

bool Proflame2::get_switch_is_on(Proflame2Function function) {
  switch (function) {
    case Proflame2Function::AUX:
      return this->is_on() && this->get_aux_is_on();
      break;
    case Proflame2Function::PILOT_LIGHT:
      return this->get_pilot_light();
      break;
    case Proflame2Function::SPLIT_FLOW:
      return this->is_on() && this->get_split_flow_is_on();
      break;
    default:
      return false;
  }
}

void Proflame2::set_switch(Proflame2Function function, bool on) {
  switch (function) {
    case Proflame2Function::AUX:
      this->set_aux_on(on);
      break;
    case Proflame2Function::PILOT_LIGHT:
      this->enable_continuous_pilot(on);
      break;
    case Proflame2Function::SPLIT_FLOW:
      this->enable_split_flow(on);
      break;
    default:
      return;
  }
}

#ifdef USE_BINARY_SENSOR
void Proflame2::update_control_sensor_() {
  if (this->control_binary_sensor_ != nullptr) {
    switch (this->state_.control) {
      case Proflame2ControlState::UART:
        this->control_binary_sensor_->publish_state(true);
        break;
      case Proflame2ControlState::EXTERNAL:
        this->control_binary_sensor_->publish_state(false);
        break;
      case Proflame2ControlState::UNKNOWN:
      default:
        this->control_binary_sensor_->set_has_state(false);
    }
  }
}
#endif

// Helper functions for checksum calculation and validation

uint8_t Proflame2::calculate_checksum(Proflame2Packet *packet) {
  uint8_t checksum = 0;
  std::array<uint8_t, 6> raw = std::bit_cast<std::array<uint8_t, 6>>(*packet);
  // XOR all bytes except the checksum byte itself
  for (size_t i = 0; i < sizeof(raw) - 1; i++) {
    checksum ^= raw[i];
  }
  return checksum;
}

bool Proflame2::validate_checksum(Proflame2Packet *packet) { return calculate_checksum(packet) == packet->checksum; }

// Command processing

void Proflame2::send_raw_command_(Proflame2Command command) {
  Proflame2Packet packet;
  packet.cmd = command;
  packet.checksum = calculate_checksum(&packet);
  this->last_command_timestamp_ = App.get_loop_component_start_time();
  this->write_array(std::bit_cast<std::array<uint8_t, 6>>(packet));
  this->flush();
}

void Proflame2::process_command_queue_() {
  if (this->command_queue_.empty()) {
    // Poll only if we're not busy doing other things.
    // This gives precedence to state change commands and avoids
    // race conditions with stale data coming in just as we change states.
    if (this->poll_registers_.CONTROL)
      this->poll_state_(Proflame2Register::CONTROL, true);

    if (this->poll_registers_.STATE_1)
      this->poll_state_(Proflame2Register::STATE_1, true);

    if (this->poll_registers_.STATE_2)
      this->poll_state_(Proflame2Register::STATE_2, true);

    return;
  }

  uint32_t now = App.get_loop_component_start_time();
  auto cmd = this->command_queue_.front();
  bool may_send = this->last_command_timestamp_ + COMMAND_DELAY < now;

  if (cmd.timestamp == 0) {
    if (may_send) {
      cmd.timestamp = now;
      this->send_raw_command_(cmd.command);
    }
    return;
  }

  uint32_t msg_age = now - cmd.timestamp;

  if (msg_age > RECEIVE_TIMEOUT) {
    ESP_LOGW(TAG, "Command %s(%s) timeout, command discarded", to_string(cmd.command.type).c_str(),
             to_string(cmd.command.reg).c_str());
    this->command_queue_.pop_front();
    return;
  }

  if (may_send) {
    ESP_LOGV(TAG, "Retrying command %s(%s) after delay (message age %d ms)", to_string(cmd.command.type).c_str(),
             to_string(cmd.command.reg).c_str(), msg_age);
    this->send_raw_command_(cmd.command);
  }
}

void Proflame2::send_command_(const Proflame2Command &command) {
  this->command_queue_.push_back({.command = command});
  this->process_command_queue_();
}

void Proflame2::send_state_() {
  if (this->last_reported_state_ == this->state_) {
    ESP_LOGV(TAG, "State has not changed, no need to send update");
    return;
  }

  auto state = std::bit_cast<std::array<uint8_t, 3>>(this->state_);

  if (!(state[0] & POWER_ON_ONLY_FEATURES_MASK) && state[1] == 0x00) {
    ESP_LOGV(TAG, "No fireplace feature remains on, setting power state to off");
    state[0] = state[0] & (1 << CONTINUOUS_PILOT_BIT);  // Retain possible continuous pilot bit
    this->update_state1_(bit_cast<Proflame2State1>(state[0]));
  }

  if (this->state_.state1 != this->last_reported_state_.state1) {
    this->send_command_({.type = Proflame2CommandType::SET,
                         .payload = {0x00, std::bit_cast<uint8_t>(this->state_.state1), 0x00},
                         .reg = Proflame2Register::STATE_1});
  }

  if (this->state_.state2 != this->last_reported_state_.state2) {
    this->send_command_({.type = Proflame2CommandType::SET,
                         .payload = {0x00, std::bit_cast<uint8_t>(this->state_.state2), 0x00},
                         .reg = Proflame2Register::STATE_2});
  }
}

// Polling function that batches up all polling requests per cycle.
// You can set `soon` to true to send the command in the next cycle,
// or false (the default) to batch it.
void Proflame2::poll_state_(Proflame2Register reg, bool soon) {
  switch (reg) {
    case Proflame2Register::CONTROL:
      if (soon) {
        this->send_command_(Proflame2Command{.type = Proflame2CommandType::GET, .reg = Proflame2Register::CONTROL});
        this->poll_registers_.CONTROL = false;
      } else {
        this->poll_registers_.CONTROL = true;
      }
      break;
    case Proflame2Register::STATE_1:
      if (soon) {
        this->send_command_(Proflame2Command{.type = Proflame2CommandType::GET, .reg = Proflame2Register::STATE_1});
        this->poll_registers_.STATE_1 = false;
      } else {
        this->poll_registers_.STATE_1 = true;
      }
      break;
    case Proflame2Register::STATE_2:
      if (soon) {
        this->send_command_(Proflame2Command{.type = Proflame2CommandType::GET, .reg = Proflame2Register::STATE_2});
        this->poll_registers_.STATE_2 = false;
      } else {
        this->poll_registers_.STATE_2 = true;
      }
      break;
    default:
      return;
  }
}

void Proflame2::poll_state_(bool soon) {
  this->poll_state_(Proflame2Register::STATE_1, soon);
  this->poll_state_(Proflame2Register::STATE_2, soon);
}

void Proflame2::assert_control_() {
  if (this->state_.control == Proflame2ControlState::UART) {
    return;
  }

  this->send_command_({.type = Proflame2CommandType::SET, .payload = PKT_CTRL_UART, .reg = Proflame2Register::CONTROL});
  this->poll_state_(Proflame2Register::CONTROL);
}

// Response packet handling

void Proflame2::flush_listeners_() {
  for (const auto &function : this->changed_functions_) {
    for (auto &listener : this->listeners_) {
      if (listener.function == function) {
        listener.on_state_change(&this->state_);
      }
    }
  }
  this->changed_functions_.clear();
}

void Proflame2::flush_all_listeners_() {
  for (auto &listener : this->listeners_) {
    listener.on_state_change(&this->state_);
  }
}

void Proflame2::received_packet_(const Proflame2Packet *packet) {
  bool is_response = (static_cast<uint8_t>(packet->cmd.type) & RESPONSE_MASK) != 0;
  if (!is_response) {
    ESP_LOGVV(PACKET_TAG, "Received Proflame 2 packet that is not a response: %s", to_string(*packet).c_str());
    return;
  }

  if (!this->command_queue_.empty()) {
    auto cmd = this->command_queue_.front().command;
    if (cmd.reg == packet->cmd.reg && is_response) {
      this->command_queue_.pop_front();  // Our packet was acknowledged, next in line please
      if (packet->cmd.type == Proflame2CommandType::SET_RESPONSE) {
        return;  // The SET_RESPONSE payload is just an echo of the SET command.
                 // The payload is identical to the original SET command, even if the SET failed.
                 // It does not include information about the actual state, so we don't process it.
      }
    }
  }

  switch (packet->cmd.reg) {
    case Proflame2Register::CONTROL:
      this->handle_control_response_(packet);
      break;
    case Proflame2Register::STATE_1:
      this->update_state1_(std::bit_cast<Proflame2State1>(packet->cmd.payload[1]));
      break;
    case Proflame2Register::STATE_2:
      this->update_state2_(std::bit_cast<Proflame2State2>(packet->cmd.payload[1]));
      break;
    default:
      ESP_LOGVV(PACKET_TAG, "Payload with invalid register: %s", to_string(*packet).c_str());
  }
  this->last_reported_state_ = this->state_;
  this->flush_listeners_();
}

// If the value has changed, log the change and update the state.
// This is a bit repetitive but necessary to keep type-safety intact.
template<typename T> bool Proflame2::update_if_changed_(Proflame2Function func, T new_value) {
  T current_value{};
  bool changed = false;

  switch (func) {
    case Proflame2Function::CONTROL:
      if constexpr (std::is_same_v<T, Proflame2ControlState>) {
        current_value = static_cast<T>(this->state_.control);
        if (current_value != new_value) {
          this->state_.control = new_value;
          changed = true;
        }
      }
      break;
    case Proflame2Function::PILOT_LIGHT:
      if constexpr (std::is_same_v<T, bool>) {
        current_value = static_cast<T>(this->state_.state1.continuous_pilot_on);
        if (current_value != new_value) {
          this->state_.state1.continuous_pilot_on = new_value;
          changed = true;
        }
      }
      break;
    case Proflame2Function::LIGHT:
      if constexpr (std::is_same_v<T, uint8_t>) {
        current_value = static_cast<T>(this->state_.state1.light_level);
        if (current_value != new_value) {
          this->state_.state1.light_level = new_value;
          changed = true;
        }
      }
      break;
    case Proflame2Function::MODE:
      if constexpr (std::is_same_v<T, uint8_t>) {
        current_value = static_cast<T>(this->state_.state1.mode);
        if (current_value != new_value) {
          this->state_.state1.mode = static_cast<Proflame2Mode>(new_value);
          changed = true;
        }
      }
      break;
    case Proflame2Function::SPLIT_FLOW:
      if constexpr (std::is_same_v<T, bool>) {
        current_value = static_cast<T>(this->state_.state2.split_flow_on);
        if (current_value != new_value) {
          this->state_.state2.split_flow_on = new_value;
          changed = true;
        }
      }
      break;
    case Proflame2Function::COMFORT_FAN:
      if constexpr (std::is_same_v<T, uint8_t>) {
        current_value = static_cast<T>(this->state_.state2.comfort_fan_speed);
        if (current_value != new_value) {
          this->state_.state2.comfort_fan_speed = new_value;
          changed = true;
        }
      }
      break;
    case Proflame2Function::AUX:
      if constexpr (std::is_same_v<T, bool>) {
        current_value = static_cast<T>(this->state_.state2.aux_on);
        if (current_value != new_value) {
          this->state_.state2.aux_on = new_value;
          changed = true;
        }
      }
      break;
    case Proflame2Function::FLAME:
      if constexpr (std::is_same_v<T, uint8_t>) {
        current_value = static_cast<T>(this->state_.state2.flame_height);
        if (current_value != new_value) {
          this->state_.state2.flame_height = new_value;
          changed = true;
        }
      }
      break;
    default:
      return false;
  }

  if (changed) {
    ESP_LOGV(TAG, "Function %s changed from %s to %s", to_string(func).c_str(), to_string(current_value).c_str(),
             to_string(new_value).c_str());
    this->changed_functions_.insert(func);
    return true;
  }

  return false;
}

void Proflame2::handle_control_response_(const Proflame2Packet *packet) {
  if (packet->cmd.payload == PKT_CTRL_UART) {
    bool control_established = this->update_if_changed_(Proflame2Function::CONTROL, Proflame2ControlState::UART);

    if (control_established) {
      ESP_LOGV(TAG, "UART is now in control of the fireplace");
      // Control (re)established; poll all state registers immediately and then establish accurate state
      this->poll_state_(true);

      // If the remote was last used a previous state may still be set, albeit with the fireplace mode set to OFF.
      // In that case ALL of these functions would return to their previous state if ANY function needs to set the
      // fireplace mode to ON.
      // This can cause unexpected situations where, for example, the flame turns on when the user requests the
      // comfort fan to be on.
      // Therefore we set everything to off, except for the pilot light, which can be thought of
      // as more of a config value than a state.
      this->state_.state1.light_level = 0;
      this->state_.state2.aux_on = false;
      this->state_.state2.comfort_fan_speed = 0;
      this->state_.state2.flame_height = 0;
      this->state_.state2.split_flow_on = false;
      this->flush_all_listeners_();
    }
  } else if (packet->cmd.payload == PKT_CTRL_EXTERNAL) {
    bool control_lost = this->update_if_changed_(Proflame2Function::CONTROL, Proflame2ControlState::EXTERNAL);

    if (control_lost) {
      // We lost control of the IFC, clear all queued commands
      ESP_LOGW(TAG, "UART is locked out of controlling the fireplace (e.g. by another remote)");
      this->command_queue_.clear();
    }
  }
#ifdef USE_BINARY_SENSOR
  this->update_control_sensor_();
#endif
}

void Proflame2::update_state1_(Proflame2State1 state) {
  this->update_if_changed_(Proflame2Function::PILOT_LIGHT, state.continuous_pilot_on);
  this->update_if_changed_(Proflame2Function::LIGHT, state.light_level);
  this->update_if_changed_(Proflame2Function::MODE, state.mode);
}

void Proflame2::update_state2_(Proflame2State2 state) {
  this->update_if_changed_(Proflame2Function::SPLIT_FLOW, state.split_flow_on);
  this->update_if_changed_(Proflame2Function::COMFORT_FAN, state.comfort_fan_speed);
  this->update_if_changed_(Proflame2Function::AUX, state.aux_on);
  this->update_if_changed_(Proflame2Function::FLAME, state.flame_height);
}

}  // namespace proflame2
}  // namespace esphome
