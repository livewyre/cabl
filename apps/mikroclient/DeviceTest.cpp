/*
        ##########    Copyright (C) 2015 Vincenzo Pacella
        ##      ##    Distributed under MIT license, see file LICENSE
        ##      ##    or <http://opensource.org/licenses/MIT>
        ##      ##
##########      ############################################################# shaduzlabs.com #####*/

#include "DeviceTest.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include <unmidify.hpp>

#include <cabl/gfx/TextDisplay.h>

//--------------------------------------------------------------------------------------------------

namespace
{
const sl::Color kColor_Black{0};
const sl::Color kColor_Red{0xff, 0, 0, 0xff};
const sl::Color kColor_Yellow{0xff, 0xff, 0, 0x55};
const sl::Color kColor_Blue{0xff, 0, 0, 0x77};

const int max_char_width = 21;
}

//--------------------------------------------------------------------------------------------------

namespace sl
{

using namespace midi;
using namespace cabl;
using namespace std::placeholders;

//--------------------------------------------------------------------------------------------------

void DeviceTest::initDevice()
{
  M_LOG("[Device Test] Is this thing on?");
  std::string value = "Once you get into a serious dead tech collection the tendency is to take it as far as you can.";
  
//  for(unsigned i  = 0; i< device()->numOfLedArrays(); i++)
//  {
//    device()->ledArray(i)->setValue(0.5, kColor_Blue);
//  }

//  for(unsigned i  = 0; i< device()->numOfGraphicDisplays(); i++)
//  {
//    unsigned w = device()->graphicDisplay(i)->width();
//    unsigned h = device()->graphicDisplay(i)->height();
//    device()->graphicDisplay(i)->black();
//    device()->graphicDisplay(i)->line(0, 0, w, h, {0xff});
//    device()->graphicDisplay(i)->line(0, h, w, 0, {0xff});
//    device()->graphicDisplay(i)->line(w/2, h, w/2, 0, {0xff});
//    device()->graphicDisplay(i)->line(0, h/2, w, h/2, {0xff});
//    device()->graphicDisplay(i)->rectangle(0, 0, w, h, {0xff});
//    device()->graphicDisplay(i)->circle(w/2, h/2, w/2, {0xff});
//    device()->graphicDisplay(i)->circle(w/2, h/2, h/2, {0xff});
//  }
  
  if(value.length() > max_char_width) {
  	
		device()->graphicDisplay(0)->black();
		
  	std::string line = "";
  	
  	unsigned trailing_space = 0;
  	unsigned last_space = 0;
  	unsigned line_no = 0;
  	unsigned line_offset = 12;
  	unsigned actual = 0;  	
  	
  	for(unsigned i = 0; i < value.length(); i++) {
  		actual++;
  		char c = value[i];
  		
  		if(c == char(' ')) {
  			last_space = i;
  		}  		
  		  		
  		line = line + c;
  		
			if(i == value.length() - 1) {
	  			//M_LOG("[Device Test] Hit the end of the string! " + line);
					device()->graphicDisplay(0)->putText(0, line_offset * line_no, line.c_str(), {0xff});
					break;			
			}
  		
  		if(line.length() > max_char_width) {				
				// (very) crude wordwrap
				if(line.length() > max_char_width) {
					if(last_space > 0 && last_space > trailing_space) {					
						i = last_space;
						trailing_space = last_space;					
						line = line.substr(0, line.find_last_of(' '));
					}	
				}
				
				device()->graphicDisplay(0)->putText(0, line_offset * line_no, line.c_str(), {0xff});
				
				line = "";
				line_no++;
			}
			
		}
  } else {  
	  device()->graphicDisplay(0)->black();
	  device()->graphicDisplay(0)->putText(0, 10, value.c_str(), {0xff});
  }
  
}

//--------------------------------------------------------------------------------------------------

void DeviceTest::render()
{
}

//--------------------------------------------------------------------------------------------------

void DeviceTest::buttonChanged(Device::Button button_, bool buttonState_, bool shiftState_)
{
	M_LOG("[Device Test] Got a button");
  device()->setButtonLed(
    button_, buttonState_ ? (shiftState_ ? kColor_Red : kColor_Yellow) : kColor_Black);
}

//--------------------------------------------------------------------------------------------------

void DeviceTest::encoderChanged(unsigned encoder_, bool valueIncreased_, bool shiftPressed_)
{
	M_LOG("[Device Test] Got an encoder");
  std::string value = "Enc#" + std::to_string(static_cast<int>(encoder_)) + ( valueIncreased_ ? " increased" : " decreased" );

  device()->textDisplay(0)->putText(value.c_str(), 0);

  device()->graphicDisplay(0)->black();
  device()->graphicDisplay(0)->putText(10, 10, value.c_str(), {0xff});

}

//--------------------------------------------------------------------------------------------------

void DeviceTest::keyChanged(unsigned index_, double value_, bool shiftPressed_)
{
	M_LOG("[Device Test] Got a 'key' (he mean pad he just don' know no better)");
  device()->setKeyLed(index_, {static_cast<uint8_t>(value_ * 0xff)});
}

//--------------------------------------------------------------------------------------------------

void DeviceTest::controlChanged(unsigned pot_, double value_, bool shiftPressed_)
{
	M_LOG("[Device Test] Control Changed");
  std::string value = "Pot#" + std::to_string(static_cast<int>(pot_)) + " " + std::to_string(static_cast<int>(value_ * 100));

  device()->textDisplay(0)->putText(value.c_str(), 0);

  device()->graphicDisplay(0)->black();
  device()->graphicDisplay(0)->putText(10, 10, value.c_str(), {0xff});
  
  device()->ledArray(pot_)->setValue(value_, kColor_Red);
}

//--------------------------------------------------------------------------------------------------

} // namespace sl
