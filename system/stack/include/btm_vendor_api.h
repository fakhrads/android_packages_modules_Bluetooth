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

#pragma once

#include <cstdint>
#include <vector>

#include "types/raw_address.h"

/*
 * The Qualcomm side of the controller that the A2DP offload needs to know about: which add on
 * features it reports, and whether a link runs on the QHS PHY (Qualcomm High Speed), which aptX
 * Lossless needs. It talks to the controller with the vendor specific commands and events of the
 * Qualcomm Bluetooth Core Extension (QBCE), the way the CLO stack does with
 * persist.vendor.qcom.bluetooth.btconfigstore off, and only on a controller that answers.
 *
 * The LE Audio side (QLL, QHS for ISO, aptX Adaptive LE) is not part of it.
 */

// codec_specific_3 of the source codec state: QHS PHY available for the link
#define QHS_SUPPORT_MASK 0x00000C00
#define QHS_SUPPORT_NOT_AVAILABLE 0x00000400
#define QHS_SUPPORT_AVAILABLE 0x00000800

// Reads the add on features of the controller and configures QHS and the QCM event mask. Does
// nothing when the vendor property persist.vendor.qcom.bluetooth.vsc_enabled says the vendor
// Bluetooth HAL does this itself, or when the controller is not a Qualcomm one that has them.
void BTM_ReadVendorAddOnFeatures();

// A vendor specific event with the Qualcomm Bluetooth Core Extension subevent code 0x51
void btm_vendor_vse_cback(uint8_t vse_subcode, std::vector<uint8_t> payload);

// True when the ACL link to |bda| runs on the QHS PHY (BR/EDR only)
bool BTM_IsQHSPhySupported(const RawAddress& bda);

// aptX Adaptive over the controller: source split A2DP, and the TX split of it. Both come from
// the add on features of the controller, and are false while they were not read.
bool BTM_IsAptxAdaptiveSplitA2dpSupported();
bool BTM_IsAptxAdaptiveSplitTxSupported();

// Forgets everything read from the controller: used by the tests
void btm_vendor_reset_for_testing();
