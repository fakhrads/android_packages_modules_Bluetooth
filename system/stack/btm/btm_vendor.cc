/*
 * Copyright (c) 2026 Fakhri Adi Saputra
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "btm_vendor"

#include "stack/include/btm_vendor_api.h"

#include <bluetooth/log.h>
#include <strings.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>

#include "osi/include/properties.h"
#include "stack/include/acl_api.h"
#include "stack/include/bt_types.h"
#include "stack/include/btm_api_types.h"
#include "stack/include/btm_client_interface.h"
#include "stack/include/hci_error_code.h"

using namespace bluetooth;

namespace {

// Qualcomm Bluetooth Core Extension: the one vendor opcode, and its sub commands
constexpr uint16_t kHciVsQbceOcf = 0xFC51;
constexpr uint16_t kHciVsGetAddonFeatures = 0xFC1D;
constexpr uint8_t kQbceSetQhsHostMode = 0x01;
constexpr uint8_t kQbceSetQlmEventMask = 0x0F;
constexpr uint8_t kQbceSetQllEventMask = 0x10;

// What the QHS host mode takes: the transport, and the mode
constexpr uint8_t kQhsTransportBrEdr = 0;
constexpr uint8_t kQhsTransportLe = 1;
constexpr uint8_t kQhsTransportLeIso = 2;
constexpr uint8_t kQhsHostAware = 3;
constexpr uint8_t kQhsHostDisableAll = 4;
constexpr uint8_t kQhsBrEdrMask = 0x01;
constexpr uint8_t kQhsLeMask = 0x02;

// Vendor specific event sent for the QBCE: the message type it starts with
constexpr uint8_t kHciVseSubcodeQbce = 0x51;
constexpr uint8_t kMsgQbceQcmPhyChange = 0x01;

constexpr uint8_t kQcmPhyStateBrEdr = 0;
constexpr uint8_t kQcmPhyStateQhs = 1;

// Qualcomm Bluetooth SoC families, oldest first. Add on features exist from Cherokee on.
constexpr const char* kSocNames[] = {"smd",      "ar3k",   "rome",   "cherokee", "hastings",
                                     "moselle",  "hamilton", "ganges", "orne",     "evros",
                                     "cologne",  "congo",  "europa"};
constexpr int kSocCherokee = 3;

constexpr size_t kAddOnFeaturesMax = 16;

// Add on features, page 0
constexpr bool addon_byte_bit(const uint8_t* f, size_t len, size_t index, uint8_t mask) {
  return index < len && (f[index] & mask) != 0;
}

std::mutex g_mutex;
uint8_t g_addon_features[kAddOnFeaturesMax];
size_t g_addon_features_len = 0;
std::map<RawAddress, uint8_t> g_qcm_phy_state;

// Whether the SoC named in persist.vendor.qcom.bluetooth.soc has the add on features: an SoC the
// table does not know is a newer one, and counts as having them
bool soc_has_addon_features() {
  char soc_name[PROPERTY_VALUE_MAX] = {'\0'};
  osi_property_get("persist.vendor.qcom.bluetooth.soc", soc_name, "");
  if (soc_name[0] == '\0') {
    log::info("no persist.vendor.qcom.bluetooth.soc, no vendor add on features");
    return false;
  }
  for (int i = 0; i < static_cast<int>(sizeof(kSocNames) / sizeof(kSocNames[0])); i++) {
    if (strcasecmp(soc_name, kSocNames[i]) == 0) {
      log::info("Bluetooth soc {}", soc_name);
      return i >= kSocCherokee;
    }
  }
  log::info("Bluetooth soc {} is not in the table, counted as a recent one", soc_name);
  return true;
}

bool vendor_layer_in_the_hal() {
  return osi_property_get_bool("persist.vendor.qcom.bluetooth.vsc_enabled", false);
}

void qbce_command_complete(tBTM_VSC_CMPL* p_data) {
  if (p_data == nullptr || p_data->p_param_buf == nullptr || p_data->param_len < 2) {
    return;
  }
  uint8_t* stream = p_data->p_param_buf;
  uint8_t status;
  uint8_t subcmd;
  STREAM_TO_UINT8(status, stream);
  STREAM_TO_UINT8(subcmd, stream);
  log::info("QBCE sub command 0x{:02x} completed, status 0x{:02x}", subcmd, status);
}

void send_qbce(const uint8_t* data, uint8_t len) {
  uint8_t command[9];
  if (len > sizeof(command)) {
    return;
  }
  memcpy(command, data, len);
  get_btm_client_interface().vendor.BTM_VendorSpecificCommand(kHciVsQbceOcf, len, command,
                                                              qbce_command_complete);
}

// The QHS host modes and the event masks, in the order and with the bytes of the Qualcomm stack. The
// event masks go out with the least significant byte first: 0x4A 0x02 asks for the messages 1 (QCM PHY
// change), 3, 6 and 9. The first version of this sent them the other way round, and the controller
// then never reported a link going to QHS.
void configure_qhs() {
  bool qle_supported;
  bool qcm_supported;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    qle_supported = addon_byte_bit(g_addon_features, g_addon_features_len, 3, 0x10);
    qcm_supported = addon_byte_bit(g_addon_features, g_addon_features_len, 3, 0x20);
  }

  if (qle_supported) {
    // QHS for LE ISO is on unless persist.vendor.qcom.bluetooth.qhs_enable says otherwise
    char qhs_iso[PROPERTY_VALUE_MAX] = "true";
    osi_property_get("persist.vendor.qcom.bluetooth.qhs_enable", qhs_iso, "true");
    const uint8_t iso_mode[] = {kQbceSetQhsHostMode, kQhsTransportLeIso,
                                static_cast<uint8_t>(strncmp("true", qhs_iso, 4) == 0
                                                             ? kQhsHostAware
                                                             : kQhsHostDisableAll)};
    send_qbce(iso_mode, sizeof(iso_mode));

    // A test and debug property of the vendor: 0xFF leaves the controller's own default
    char qhs_value[PROPERTY_VALUE_MAX] = "255";
    osi_property_get("persist.vendor.qcom.bluetooth.qhs_support", qhs_value, "255");
    uint8_t qhs_support_mask = static_cast<uint8_t>(atoi(qhs_value));
    log::info("qhs support mask={}", qhs_support_mask);
    if (qhs_support_mask != 0xFF) {
      const uint8_t br_edr[] = {kQbceSetQhsHostMode, kQhsTransportBrEdr,
                                static_cast<uint8_t>((qhs_support_mask & kQhsBrEdrMask)
                                                             ? kQhsHostAware
                                                             : kQhsHostDisableAll)};
      send_qbce(br_edr, sizeof(br_edr));
      const uint8_t le_mode = (qhs_support_mask & kQhsLeMask) ? kQhsHostAware : kQhsHostDisableAll;
      const uint8_t le[] = {kQbceSetQhsHostMode, kQhsTransportLe, le_mode};
      send_qbce(le, sizeof(le));
      const uint8_t le_iso[] = {kQbceSetQhsHostMode, kQhsTransportLeIso, le_mode};
      send_qbce(le_iso, sizeof(le_iso));
    } else {
      // Not in the stock stack, which leaves BR/EDR at the controller default: QHS is asked for
      // explicitly on BR/EDR links, for aptX Adaptive Lossless, and LE is left as it is.
      const uint8_t br_edr[] = {kQbceSetQhsHostMode, kQhsTransportBrEdr, kQhsHostAware};
      send_qbce(br_edr, sizeof(br_edr));
    }

    const uint8_t qll_event_mask[] = {kQbceSetQllEventMask, 0x4A, 0x02, 0x00, 0x00,
                                      0x00,                 0x00, 0x00, 0x00};
    send_qbce(qll_event_mask, sizeof(qll_event_mask));
  }

  if (qcm_supported) {
    const uint8_t qlm_event_mask[] = {kQbceSetQlmEventMask, 0x4A, 0x02, 0x00, 0x00,
                                      0x00,                 0x00, 0x00, 0x00};
    send_qbce(qlm_event_mask, sizeof(qlm_event_mask));
  } else {
    log::info("the controller has no QCM HCI, so it does not report the PHY of a link");
  }
}

void addon_features_complete(tBTM_VSC_CMPL* p_data) {
  if (p_data == nullptr || p_data->p_param_buf == nullptr) {
    return;
  }
  uint8_t* stream = p_data->p_param_buf;
  uint16_t length = p_data->param_len;
  uint8_t status;
  STREAM_TO_UINT8(status, stream);
  if (status != HCI_SUCCESS) {
    log::warn("the controller refused the add on features, status 0x{:02x}", status);
    return;
  }
  // status, product id (2), response version (2), then the features
  if (length <= 5) {
    log::warn("add on features response too short: {}", length);
    return;
  }
  uint16_t product_id;
  uint16_t response_version;
  STREAM_TO_UINT16(product_id, stream);
  STREAM_TO_UINT16(response_version, stream);
  size_t len = std::min<size_t>(length - 5, kAddOnFeaturesMax);
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    memset(g_addon_features, 0, sizeof(g_addon_features));
    memcpy(g_addon_features, stream, len);
    g_addon_features_len = len;
  }
  log::info("add on features: product 0x{:04x}, version {}, {} bytes", product_id,
            response_version, len);
  configure_qhs();
}

void qcm_phy_change(const uint8_t* p, size_t len) {
  // status (1), connection handle (2), PHY state (1)
  if (len < 4) {
    return;
  }
  uint8_t status = p[0];
  uint16_t handle = static_cast<uint16_t>(p[1] | (p[2] << 8)) & 0x0FFF;
  uint8_t state = p[3];

  const RawAddress bda = acl_address_from_handle(handle);
  if (bda == RawAddress::kEmpty) {
    log::warn("no ACL link for handle 0x{:04x}", handle);
    return;
  }
  std::lock_guard<std::mutex> lock(g_mutex);
  if (status != HCI_SUCCESS) {
    log::warn("QCM PHY change failed for {}, status 0x{:02x}", bda, status);
    g_qcm_phy_state[bda] = kQcmPhyStateBrEdr;
    return;
  }
  log::info("{} is on the {} PHY", bda, state == kQcmPhyStateQhs ? "QHS" : "BR/EDR");
  g_qcm_phy_state[bda] = state;
}

}  // namespace

void BTM_ReadVendorAddOnFeatures() {
  if (vendor_layer_in_the_hal()) {
    log::info("the vendor Bluetooth HAL owns the vendor commands");
    return;
  }
  if (!soc_has_addon_features()) {
    return;
  }
  get_btm_client_interface().vendor.BTM_VendorSpecificCommand(kHciVsGetAddonFeatures, 0, nullptr,
                                                              addon_features_complete);
}

void btm_vendor_vse_cback(uint8_t vse_subcode, std::vector<uint8_t> payload) {
  if (vse_subcode != kHciVseSubcodeQbce || payload.empty()) {
    return;
  }
  const uint8_t message = payload[0];
  log::info("QBCE message 0x{:02x}, {} bytes", message, payload.size());
  switch (message) {
    case kMsgQbceQcmPhyChange:
      qcm_phy_change(payload.data() + 1, payload.size() - 1);
      break;
    default:
      // The LE side of the extension (QLL, CIS latency, codec settings) is not handled here
      log::debug("QBCE message 0x{:02x} ignored", message);
      break;
  }
}

bool BTM_IsQHSPhySupported(const RawAddress& bda) {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_qcm_phy_state.find(bda);
  return it != g_qcm_phy_state.end() && it->second == kQcmPhyStateQhs;
}

bool BTM_IsAptxAdaptiveSplitA2dpSupported() {
  std::lock_guard<std::mutex> lock(g_mutex);
  return addon_byte_bit(g_addon_features, g_addon_features_len, 1, 0x40);
}

bool BTM_IsAptxAdaptiveSplitTxSupported() {
  std::lock_guard<std::mutex> lock(g_mutex);
  return addon_byte_bit(g_addon_features, g_addon_features_len, 3, 0x80);
}

void btm_vendor_reset_for_testing() {
  std::lock_guard<std::mutex> lock(g_mutex);
  memset(g_addon_features, 0, sizeof(g_addon_features));
  g_addon_features_len = 0;
  g_qcm_phy_state.clear();
}
