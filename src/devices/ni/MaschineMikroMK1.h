/*
        ##########    Copyright (C) 2015 Vincenzo Pacella
        ##      ##    Distributed under MIT license, see file LICENSE
        ##      ##    or <http://opensource.org/licenses/MIT>
        ##      ##
##########      ############################################################# shaduzlabs.com #####*/

#pragma once

#include <array>
#include <bitset>

#include "cabl/comm/Transfer.h"
#include "cabl/devices/Device.h"
#include "cabl/devices/DeviceFactory.h"

namespace sl
{
namespace cabl
{

class MaschineMikroMK1 : public Device
{
public:
  MaschineMikroMK1();

  void setButtonLed(Device::Button, const Color&) override;
  void setKeyLed(unsigned, const Color&) override;

  size_t numOfGraphicDisplays() const override
  {
    return 0;
  }
  size_t numOfTextDisplays() const override
  {
    return 0;
  }
  size_t numOfLedMatrices() const override
  {
    return 0;
  }
  size_t numOfLedArrays() const override
  {
    return 0;
  }

  bool tick() override;

private:
  void init() override;

  bool read();
  void processReport(const Transfer&);
  void processButtonsReport(const Transfer&);
  void processPadsLikeMk1Mk2(const Transfer&);

  static constexpr uint8_t kNumPads = 16;
  static constexpr unsigned kNumButtonBits = 32; // 4 bytes * 8 bits

  std::array<unsigned, kNumPads> m_padValues{};
  std::bitset<kNumPads> m_padDown{};

  std::bitset<kNumButtonBits> m_buttonDown{};
  int m_buttonsActiveLow{-1}; // -1 unknown, 0 active-high, 1 active-low
  bool m_encoderInitialized{false};
  uint8_t m_encoderValue{0};
};

M_REGISTER_DEVICE_CLASS(
  MaschineMikroMK1, "Maschine Mikro MK1", DeviceDescriptor::Type::HID, 0x17CC, 0x1110);

} // namespace cabl
} // namespace sl

