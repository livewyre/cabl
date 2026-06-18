/*
        ##########    Copyright (C) 2015 Vincenzo Pacella
        ##      ##    Distributed under MIT license, see file LICENSE
        ##      ##    or <http://opensource.org/licenses/MIT>
        ##      ##
##########      ############################################################# shaduzlabs.com #####*/

#include "devices/ni/MaschineMikroMK1.h"

#include <algorithm>
#include <bitset>
#include <sstream>

#include "cabl/util/Log.h"

namespace
{
constexpr unsigned kPadThreshold = 200;

// Raw pad index (from device report) -> logical pad index (0..15) in physical order 1..16.
// Derived from the user's test where they played pads 1..16 and the device reported:
// 12,13,14,15,8,9,10,11,4,5,6,7,0,1,2,3
constexpr std::array<uint8_t, 16> kRawPadToLogical = {12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3};

struct ButtonMapping
{
  sl::cabl::Device::Button id;
  const char* name;
};

// Button banks observed on Mikro MK1 buttons report (0x01 len=6):
// payload bytes = 5, where the last payload byte behaves like encoder position (0x0..0xF).
// The first 4 payload bytes are bitfields. We map them as bank 4..1.
static const ButtonMapping kButtonByBankBit[5][8] = {
  // bank 0 (unused)
  {{sl::cabl::Device::Button::Unknown, "Unknown"},
    {sl::cabl::Device::Button::Unknown, "Unknown"},
    {sl::cabl::Device::Button::Unknown, "Unknown"},
    {sl::cabl::Device::Button::Unknown, "Unknown"},
    {sl::cabl::Device::Button::Unknown, "Unknown"},
    {sl::cabl::Device::Button::Unknown, "Unknown"},
    {sl::cabl::Device::Button::Unknown, "Unknown"},
    {sl::cabl::Device::Button::Unknown, "Unknown"}},
  // bank 1
  {{sl::cabl::Device::Button::Mute, "Mute"},
    {sl::cabl::Device::Button::Solo, "Solo"},
    {sl::cabl::Device::Button::Select, "Select"},
    {sl::cabl::Device::Button::Duplicate, "Duplicate"},
    {sl::cabl::Device::Button::View, "View"},
    {sl::cabl::Device::Button::PadMode, "Pad Mode"},
    {sl::cabl::Device::Button::Pattern, "Pattern"},
    {sl::cabl::Device::Button::Scene, "Scene"}},
  // bank 2
  {{sl::cabl::Device::Button::Enter, "Enter"},
    {sl::cabl::Device::Button::NavigateRight, "Right Nav"},
    {sl::cabl::Device::Button::NavigateLeft, "Left Nav"},
    {sl::cabl::Device::Button::Nav, "Nav"},
    {sl::cabl::Device::Button::Main, "Main"},
    {sl::cabl::Device::Button::F3, "F3"},
    {sl::cabl::Device::Button::F2, "F2"},
    {sl::cabl::Device::Button::F1, "F1"}},
  // bank 3
  {{sl::cabl::Device::Button::Unknown, "Unknown"},
    {sl::cabl::Device::Button::Unknown, "Unknown"},
    {sl::cabl::Device::Button::Unknown, "Unknown"},
    {sl::cabl::Device::Button::MainEncoder, "Encoder Press"},
    {sl::cabl::Device::Button::NoteRepeat, "Note Repeat"},
    {sl::cabl::Device::Button::Group, "Group"},
    {sl::cabl::Device::Button::Sampling, "Sampling"},
    {sl::cabl::Device::Button::Browse, "Browse"}},
  // bank 4
  {
    {sl::cabl::Device::Button::Shift, "Shift"},                   // bit 0
    {sl::cabl::Device::Button::Erase, "Erase"},                   // bit 1
    {sl::cabl::Device::Button::Rec, "Record"},                    // bit 2
    {sl::cabl::Device::Button::Play, "Play"},                     // bit 3
    {sl::cabl::Device::Button::Grid, "Grid"},                     // bit 4
    {sl::cabl::Device::Button::TransportRight, "Right Transport"}, // bit 5
    {sl::cabl::Device::Button::TransportLeft, "Left Transport"},   // bit 6
    {sl::cabl::Device::Button::Restart, "Restart"},               // bit 7
  },
};

std::string toHexBytes(const sl::cabl::Transfer& input)
{
  std::ostringstream os;
  os << std::hex;
  for (size_t i = 0; i < input.size(); ++i)
  {
    os << (i == 0 ? "" : " ") << static_cast<int>(input[static_cast<int>(i)]);
  }
  return os.str();
}
} // namespace

namespace sl
{
namespace cabl
{

MaschineMikroMK1::MaschineMikroMK1() = default;

void MaschineMikroMK1::init()
{
  std::fill(m_padValues.begin(), m_padValues.end(), 0U);
  m_padDown.reset();

  m_buttonDown.reset();
  m_buttonsActiveLow = -1;

  m_encoderInitialized = false;
  m_encoderValue = 0;
}

void MaschineMikroMK1::setButtonLed(Device::Button, const Color&)
{
  // LEDs intentionally not implemented here (LED probing/experiments reverted).
}

void MaschineMikroMK1::setKeyLed(unsigned, const Color&)
{
  // LEDs intentionally not implemented here (LED probing/experiments reverted).
}

bool MaschineMikroMK1::tick()
{
  return read();
}

bool MaschineMikroMK1::read()
{
  Transfer input;
  if (!readFromDeviceHandle(input, 0))
  {
    return false;
  }

  if (!input)
  {
    return true;
  }

  processReport(input);
  return true;
}

void MaschineMikroMK1::processReport(const Transfer& input_)
{
  if (!input_)
  {
    return;
  }

  // Report 0x01: buttons + encoder position.
  if (input_.size() == 6 && input_[0] == 0x01)
  {
    processButtonsReport(input_);
    return;
  }

  // Pads: MK1/MK2-style encoding (0x20 + pairs) used by several NI devices in this repo.
  if (input_.size() >= 2 && input_[0] == 0x20)
  {
    processPadsLikeMk1Mk2(input_);
    return;
  }

  // Some firmwares might omit the message-type byte; try the same decode if size matches.
  if (input_.size() >= 33 && (input_.size() % 2 == 1))
  {
    processPadsLikeMk1Mk2(input_);
    return;
  }
}

void MaschineMikroMK1::processButtonsReport(const Transfer& input_)
{
  const size_t nPayloadBytes = input_.size() - 1;
  if (nPayloadBytes < 5)
  {
    return;
  }

  // Determine active-low vs active-high once, using a simple heuristic.
  if (m_buttonsActiveLow < 0)
  {
    bool allZero = true;
    bool allFF = true;
    unsigned ones = 0;
    for (size_t i = 0; i < nPayloadBytes - 1; ++i) // exclude encoder byte
    {
      const uint8_t b = input_[static_cast<int>(1 + i)];
      allZero = allZero && (b == 0x00);
      allFF = allFF && (b == 0xFF);
      ones += static_cast<unsigned>(__builtin_popcount(static_cast<unsigned>(b)));
    }

    if (allFF)
    {
      m_buttonsActiveLow = 1;
    }
    else if (allZero)
    {
      m_buttonsActiveLow = 0;
    }
    else
    {
      const unsigned totalBits = static_cast<unsigned>((nPayloadBytes - 1) * 8);
      m_buttonsActiveLow = (ones > (totalBits / 2)) ? 1 : 0;
    }
  }

  // Build new "down" set from the 4 bitfield bytes (exclude last encoder byte).
  std::bitset<kNumButtonBits> newDown;
  for (size_t payloadByteIndex = 0; payloadByteIndex < 4; ++payloadByteIndex)
  {
    const uint8_t b = input_[static_cast<int>(1 + payloadByteIndex)];
    for (unsigned bit = 0; bit < 8; ++bit)
    {
      const unsigned buttonIndex = static_cast<unsigned>(payloadByteIndex * 8 + bit);
      const bool bitIsOne = ((b >> bit) & 0x01) != 0;
      const bool pressed = (m_buttonsActiveLow > 0) ? (!bitIsOne) : bitIsOne;
      newDown.set(buttonIndex, pressed);
    }
  }

  // Shift is bank 4, bit 0 => payload byte 0, bit 0 => buttonIndex 0.
  const bool shiftPressed = newDown.test(0);

  // Encoder position: last payload byte low nibble (0x0..0xF).
  {
    const uint8_t raw = input_[static_cast<int>(1 + (nPayloadBytes - 1))];
    const uint8_t current = static_cast<uint8_t>(raw & 0x0F);
    if (!m_encoderInitialized)
    {
      m_encoderInitialized = true;
      m_encoderValue = current;
    }
    else if (current != m_encoderValue)
    {
      const bool increased = ((m_encoderValue < current)
                               || ((m_encoderValue == 0x0F) && (current == 0x00)))
                             && (!((m_encoderValue == 0x0) && (current == 0x0F)));
      encoderChanged(0, increased, shiftPressed);
      m_encoderValue = current;
    }
  }

  for (unsigned buttonIndex = 0; buttonIndex < kNumButtonBits; ++buttonIndex)
  {
    if (newDown.test(buttonIndex) == m_buttonDown.test(buttonIndex))
    {
      continue;
    }

    const unsigned payloadByteIndex = (buttonIndex / 8); // 0..3
    const unsigned bitIndex = (buttonIndex % 8);

    const int bank = static_cast<int>(4 - payloadByteIndex); // byte0->bank4, byte3->bank1
    const auto mapping = kButtonByBankBit[bank][bitIndex];
    const bool pressed = newDown.test(buttonIndex);

    // Print raw info to help verify mapping.
    M_LOG("[MaschineMikroMK1] button: bank=" << bank << " bit=" << bitIndex << " pressed="
                                            << (pressed ? 1 : 0) << " name=" << mapping.name
                                            << " bytes=" << toHexBytes(input_));

    if (mapping.id != Device::Button::Unknown)
    {
      buttonChanged(mapping.id, pressed, shiftPressed);
    }
  }

  m_buttonDown = newDown;
}

void MaschineMikroMK1::processPadsLikeMk1Mk2(const Transfer& input_)
{
  // Layout: pairs of bytes (l,h) starting at byte 1, where:
  // rawPad = (h & 0xF0) >> 4, value = ((h & 0x0F) << 8) | l.
  for (size_t i = 1; (i + 1) < input_.size(); i += 2)
  {
    const unsigned l = input_[static_cast<int>(i)];
    const unsigned h = input_[static_cast<int>(i + 1)];
    const uint8_t rawPad = static_cast<uint8_t>((h & 0xF0) >> 4);
    if (rawPad >= kNumPads)
    {
      continue;
    }

    const uint8_t pad = kRawPadToLogical[rawPad];
    const unsigned value = (((h & 0x0F) << 8) | l);
    m_padValues[pad] = value;

    if (value > kPadThreshold)
    {
      m_padDown.set(pad, true);
      keyChanged(pad, value / 1024.0, false);
    }
    else if (m_padDown.test(pad))
    {
      m_padDown.set(pad, false);
      keyChanged(pad, 0.0, false);
    }
  }
}

} // namespace cabl
} // namespace sl

