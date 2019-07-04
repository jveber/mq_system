// Copyright: (c) Jaromir Veber 2017
// Version: 02012019
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************

#include "Options.h"        // Zwave options object
#include "Manager.h"        // Zwave manager object
#include "Notification.h"

// config++
#include <libconfig.h++>

#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <limits>
#include <algorithm>

using namespace OpenZWave;
using namespace libconfig;

Manager * g_manager;
static const char* kDefaultDriverPath = "/dev/ttyACM0";
std::unordered_map<uint8_t,std::vector<ValueID>> g_network_device_value_map;
std::atomic<uint32_t> g_home_id;
std::mutex g_mtx;

const char *value_genre_to_str (ValueID::ValueGenre vg)
{
  static const unordered_map<ValueID::ValueGenre, const char*> val_gen_map = {
    {ValueID::ValueGenre_Basic, "basic"},
    {ValueID::ValueGenre_User, "user"},
    {ValueID::ValueGenre_Config, "config"},
    {ValueID::ValueGenre_System, "system"}
  };
  const auto& iterator = val_gen_map.find(vg);
  if (iterator == val_gen_map.cend()){
    return "unknown";
  }
  return iterator->second;
}

const char *value_type_to_str (ValueID::ValueType vt)
{
  switch (vt) {
  case ValueID::ValueType_Bool:
    return "bool";
  case ValueID::ValueType_Byte:
    return "byte";
  case ValueID::ValueType_Decimal:
    return "decimal";
  case ValueID::ValueType_Int:
    return "int";
  case ValueID::ValueType_List:
    return "list";
  case ValueID::ValueType_Schedule:
    return "schedule";
  case ValueID::ValueType_String:
    return "string";
  case ValueID::ValueType_Short:
    return "short";
  case ValueID::ValueType_Button:
    return "button";
  case ValueID::ValueType_Raw:
    return "raw";
  default:
    return "unknown";
  }
}

// actual list at http://zwavepublic.com
// I used http://zwavepublic.com/sites/default/files/command_class_specs_2017A/SDS13548-4%20List%20of%20defined%20Z-Wave%20Command%20Classes.pdf
const char* command_class_value_to_str(uint8_t val){
    switch (val) {
  case 0x00:
    return "NO OPERATION";
  case 0x20:
    return "BASIC";
  case 0x21:
    return "CONTROLLER REPLICATION";
  case 0x22:
    return "APPLICATION STATUS";
  case 0x23:
    return "ZIP SERVICES";
  case 0x24:
    return "ZIP SERVER";
  case 0x25:
    return "SWITCH BINARY";
  case 0x26:
    return "SWITCH MULTILEVEL";
  case 0x27:
    return "SWITCH ALL";
  case 0x28:
    return "SWITCH TOGGLE BINARY";
  case 0x29:
    return "SWITCH TOGGLE MULTILEVEL";
  case 0x2A:
    return "CHIMNEY FAN";
  case 0x2B:
    return "SCENE ACTIVATION";
  case 0x2C:
    return "SCENE ACTUATOR CONF";
  case 0x2D:
    return "SCENE CONTROLLER CONF";
  case 0x2E:
    return "ZIP CLIENT";
  case 0x2F:
    return "ZIP ADV SERVICES";
  case 0x30:
    return "SENSOR BINARY";
  case 0x31:
    return "SENSOR MULTILEVEL";
  case 0x32:
    return "METER";
  case 0x33:
    return "COLOR";
  case 0x34:
    return "ZIP ADV CLIENT";
  case 0x35:
    return "METER PULSE";
  case 0x38:
    return "THERMOSTAT HEATING";
  case 0x40:
    return "THERMOSTAT MODE";
  case 0x42:
    return "THERMOSTAT OPERATING STATE";
  case 0x43:
    return "THERMOSTAT SETPOINT";
  case 0x44:
    return "THERMOSTAT FAN MODE";
  case 0x45:
    return "THERMOSTAT FAN STATE";
  case 0x46:
    return "CLIMATE CONTROL SCHEDULE";
  case 0x47:
    return "THERMOSTAT SETBACK";
  case 0x4C:
    return "DOOR LOCK LOGGING";
  case 0x4E:
    return "SCHEDULE ENTRY LOCK";
  case 0x50:
    return "BASIC WINDOW COVERING";
  case 0x51:
    return "MTP WINDOW COVERING";
  case 0x56:
    return "CRC16 ENCAP";
  case 0x5A:
    return "DEVICE RESET LOCALLY";
  case 0x5B:
    return "CENTRAL SCENE";
  case 0x5E:
    return "ZWAVE PLUS INFO";
  case 0x60:
    return "MULTI INSTANCE";
  case 0x62:
    return "DOOR LOCK";
  case 0x63:
    return "USER CODE";
  case 0x66:
    return "BARRIER OPERATOR";
  case 0x70:
    return "CONFIGURATION";
  case 0x71:
    return "ALARM";
  case 0x72:
    return "MANUFACTURER SPECIFIC";
  case 0x73:
    return "POWERLEVEL";
  case 0x75:
    return "PROTECTION";
  case 0x76:
    return "LOCK";
  case 0x77:
    return "NODE NAMING";
  case 0x7A:
    return "FIRMWARE UPDATE MD";
  case 0x7B:
    return "GROUPING NAME";
  case 0x7C:
    return "REMOTE ASSOCIATION ACTIVATE";
  case 0x7D:
    return "REMOTE ASSOCIATION";
  case 0x80:
    return "BATTERY";
  case 0x81:
    return "CLOCK";
  case 0x82:
    return "HAIL";
  case 0x84:
    return "WAKE UP";
  case 0x85:
    return "ASSOCIATION";
  case 0x86:
    return "VERSION";
  case 0x87:
    return "INDICATOR";
  case 0x88:
    return "PROPRIETARY";
  case 0x89:
    return "LANGUAGE";
  case 0x8A:
    return "TIME";
  case 0x8B:
    return "TIME PARAMETERS";
  case 0x8C:
    return "GEOGRAPHIC LOCATION";
  case 0x8D:
    return "COMPOSITE";
  case 0x8E:
    return "MULTI INSTANCE ASSOCIATION";
  case 0x8F:
    return "MULTI CMD";
  case 0x90:
    return "ENERGY PRODUCTION";
  case 0x91:
    return "MANUFACTURER PROPRIETARY";
  case 0x92:
    return "SCREEN MD";
  case 0x93:
    return "SCREEN ATTRIBUTES";
  case 0x94:
    return "SIMPLE AV CONTROL";
  case 0x95:
    return "AV CONTENT DIRECTORY MD";
  case 0x96:
    return "AV RENDERER STATUS";
  case 0x97:
    return "AV CONTENT SEARCH MD";
  case 0x98:
    return "SECURITY";
  case 0x99:
    return "AV TAGGING MD";
  case 0x9A:
    return "IP CONFIGURATION";
  case 0x9B:
    return "ASSOCIATION COMMAND CONFIGURATION";
  case 0x9C:
    return "SENSOR ALARM";
  case 0x9D:
    return "SILENCE ALARM";
  case 0x9E:
    return "SENSOR CONFIGURATION";
  case 0xEF:
    return "MARK";
  case 0xF0:
    return "NON INTEROPERABLE";
  default:
	return "UNKNOWN";
  }
}

static void OnNotification (Notification const *pNotification, void *_context)
{
  switch (pNotification->GetType()) {
    case Notification::NotificationType::Type_NodeAdded: {
      std::lock_guard<std::mutex> lck (g_mtx);
      g_network_device_value_map.insert(std::make_pair(pNotification->GetNodeId(), std::vector<ValueID>()));
      break;
    }
    case Notification::NotificationType::Type_DriverReady: {
      std::lock_guard<std::mutex> lck (g_mtx);
      g_home_id = pNotification->GetHomeId();
      break;
    }
    case Notification::NotificationType::Type_ValueAdded: {
      std::lock_guard<std::mutex> lck (g_mtx);
      auto value_map = g_network_device_value_map.find(pNotification->GetNodeId());
      if (value_map == g_network_device_value_map.end()) {
        printf("Warn: value already added in the map - unexpected behavior!\n");
        break;
      }
      value_map->second.push_back(pNotification->GetValueID());
    }
    default:
      break;
  }
}

void print_info() {
  std::lock_guard<std::mutex> lck (g_mtx);
  for (const auto& value_map_iterator : g_network_device_value_map) {
    bool info = g_manager->IsNodeInfoReceived(g_home_id, value_map_iterator.first);
    if (!info) {
      printf("******************************************************************************************************************************\n");
      printf("Node id %d info not received!\n", value_map_iterator.first);
      printf("******************************************************************************************************************************\n");
      continue;
    }
    printf("******************************************************************************************************************************\n");
    printf("Node id %d, device %s by %s\n" , value_map_iterator.first, g_manager->GetNodeProductName(g_home_id, value_map_iterator.first).c_str(), g_manager->GetNodeManufacturerName(g_home_id, value_map_iterator.first).c_str());
    printf("******************************************************************************************************************************\n");
    for (auto const& value : value_map_iterator.second) {
      printf("\tValue 0x%llX | label %s\nHelp: %s\n", value.GetId(), g_manager->GetValueLabel(value).c_str(), g_manager->GetValueHelp(value).c_str());
      std::string value_as_string;
      g_manager->GetValueAsString(value, &value_as_string);
      printf("\tCurrent Value %s %s \n of type %s\n", value_as_string.c_str(), g_manager->GetValueUnits(value).c_str(), value_type_to_str(value.GetType()));
      if (g_manager->IsValueReadOnly(value)) {
        printf("Read Only\n");
      } else if (g_manager->IsValueWriteOnly(value)) {
        printf("Write Only\n");
      } else {
        printf("Read-Write\n");
      }
      printf("\t----------------------------------\n");
    }
  }
}

void read_config_file(std::string& zw_driver_path, std::string& zw_config_path, std::string& zw_network_data_path) {
    static const char* kConfig_file = "/etc/mq_system/mq_zwave_daemon.conf";
    static const char* kDefaultDriverPath = "/dev/ttyACM0";
    static const char* kDefaultConfigPath = "/usr/config/";
    static const char* kDefaultLogFile = "/var/log/mq_system/zwave.log";
    static const char* kDefaultNetworkDataPath = "/etc/mq_system/";
    Config cfg;
    
    try {
        cfg.readFile(kConfig_file);
    } catch (const FileIOException &fioex) {
        throw std::runtime_error(std::string("I/O error while reading configuration file."));
    } catch (const ParseException &pex) {
        throw std::runtime_error(std::string("Parse error at ") + pex.getFile() + ":" + std::to_string(pex.getLine()) + " - " + pex.getError() );
    }
    try {
        if (!cfg.exists("driver_path"))
            zw_driver_path = kDefaultDriverPath;
        else
            cfg.lookupValue("driver_path", zw_driver_path);
        if (!cfg.exists("config_path"))
            zw_config_path = kDefaultConfigPath;
        else
            cfg.lookupValue("config_path", zw_config_path);
        if (!cfg.exists("network_data_path"))
            zw_network_data_path = kDefaultNetworkDataPath;
        else
            cfg.lookupValue("network_data_path", zw_network_data_path);
    } catch (const SettingNotFoundException &nfex) {
        throw std::runtime_error(std::string("Setting \"sensors\" not found in configuration file - running daemon without registered sensors is wast of your resources terminating"));
    } catch (const SettingTypeException &nfex) {
        throw std::runtime_error(std::string("Setting type error at: ") + nfex.getPath());
    }
}

int main() 
{
  std::string zw_driver_path, zw_config_path, zw_network_data_path;
  try {
    read_config_file(zw_driver_path, zw_config_path, zw_network_data_path);
  } catch ( std::runtime_error & e) {
    printf("Runtime error %s", e.what());
    return -1;
  }
  g_home_id = std::numeric_limits<uint32_t>::max();
  auto options = Options::Create(zw_config_path, zw_network_data_path, "");
  options->AddOptionString("LogFileName", "./Zwave.log", false);
  options->AddOptionBool("ConsoleOutput", false);
  options->AddOptionBool("AppendLogFile", false);
  options->Lock();
  g_manager = Manager::Create();
  g_manager->AddWatcher(OnNotification, nullptr);
  g_manager->AddDriver(zw_driver_path);	
  // give it some time to get g_home_id
  for (int counter = 0; g_home_id == std::numeric_limits<uint32_t>::max() && counter < 20; ++counter)
      std::this_thread::sleep_for(std::chrono::seconds(1));
  // wait at least one second to be sure that at least one "node-added" event was received 
  std::this_thread::sleep_for(std::chrono::seconds(1));
  // now wait for the nodeinfo of all nodes
  int max_node = 0;
  for (const auto& x : g_network_device_value_map) {
    if (max_node < x.first)
      max_node = x.first;
  }
  vector<bool> ready_map(max_node+1, false);
  for (int time_count_douwn = 600; time_count_douwn > 0; )
  {
    std::lock_guard<std::mutex> lck (g_mtx);
    for (const auto& x : g_network_device_value_map)
    {
      if (ready_map[x.first])
        continue;
      bool awake = g_manager->IsNodeAwake (g_home_id, x.first);
      bool info = g_manager->IsNodeInfoReceived(g_home_id, x.first);
      bool failed = g_manager->IsNodeFailed(g_home_id, x.first);
      printf("node %d", x.first);
      if (awake)
        printf(" awake");
      if (failed)
        printf(" failed");
      if (info)
        printf(" info");
      printf("\n");
      if (awake && !failed && !info) {
        g_mtx.unlock();
        printf ("Waiting for node %d to get ready %d seconds remaining\n",  x.first, time_count_douwn);
        --time_count_douwn;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        break;
      }
      ready_map[x.first] = true;
    }
    if (std::all_of(ready_map.cbegin(), ready_map.cend(), [](bool i){return i;}))
      break;
  }

  printf("Ready to print info\n");
  print_info();

  Manager::Destroy();
  Options::Destroy();
}
