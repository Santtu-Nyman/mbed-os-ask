#include "ask_CRC16.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

CRC16::CRC16(const uint16_t poly, const uint16_t init, const uint16_t xorout, const bool refin, const bool refout, CALC_METHOD calc_method) : 
poly(poly), init(init), xorout(xorout), refin(refin), refout(refout), calc_method(calc_method){	
    generateLookupTable();
}

CRC16::~CRC16(){
  if(calc_method == LOOKUP_TABLE)
  {
    free((void*)lookup_table);
  }
}

uint16_t CRC16::completeLookupCompute(const char* data, uint8_t length){
	return completeLookupCompute((uint8_t*)data, length);
}

uint16_t CRC16::completeLookupCompute(uint8_t* data, uint8_t length){
  uint16_t crc = init;  
  if(calc_method != LOOKUP_TABLE)
    return crc;

  uint8_t key;
	if(refin){
		for(uint8_t i = 0; i < length; ++i){
			crc ^= (reflect8(data[i]) << 8);
			key = crc >> 8;	
			crc <<= 8; 
			crc ^= lookup_table[key];
		}	
	}
	else if(!refin){
		for(uint8_t i = 0; i < length; ++i){
			crc ^= (data[i] << 8); 
			key = crc >> 8; 
			crc <<= 8; 
			crc ^= lookup_table[key]; 
    }	
	}
	crc = refout ? reflect16(crc) : crc;
  crc ^= xorout;
	return crc;
}

uint16_t CRC16::incompleteLookupCompute(uint16_t crc, uint8_t data){
  if(calc_method != LOOKUP_TABLE)
    return crc;
  
  if(refin)
    data = reflect8(data);
  uint16_t padded_data = data << 8;
  crc ^= padded_data; 
  uint8_t key = crc >> 8;
  crc <<= 8;
  crc ^= lookup_table[key];
  
  return crc;
}

uint16_t CRC16::incompleteBitwiseCompute(uint16_t crc, uint8_t data){  
  if(refin)
    data = reflect8(data);
  uint16_t padded_data = data << 8;
  crc ^= padded_data;
  uint16_t key = crc & 0xFF00;
  crc <<= 8;
  return crc ^ calcByte(key);
}

uint16_t CRC16::completeBitwiseCompute(const char* data, uint8_t length){
  return completeBitwiseCompute((uint8_t*)data, length);
}

uint16_t CRC16::completeBitwiseCompute(uint8_t* data, uint8_t length){
  uint16_t crc = init;
  uint16_t key;
  if(refin){
    for(uint8_t i = 0; i < length; ++i){
      crc ^= (reflect8(data[i]) << 8);
      key = crc & 0xFF00;
      crc <<= 8; 
      crc ^= calcByte(key);
    } 
  }
  else if(!refin){
    for(uint8_t i = 0; i < length; ++i){
      crc ^= (data[i] << 8); 
      key = crc >> 8; 
      crc <<= 8; 
      crc ^= calcByte(key);
    } 
  }
  crc = refout ? reflect16(crc) : crc;
  return crc ^ xorout;
}

uint16_t CRC16::complete(uint16_t crc){
	if(refout)	
		crc = reflect16(crc);
	crc ^= xorout;
	return crc;
}

uint16_t CRC16::fastCRC(uint16_t crc, uint8_t data){
    data ^= crc & 0xFF;
    data ^= data << 4;
    
    return ((((uint16_t)data << 8) | crc >> 8) ^ (uint8_t)(data >> 4) 
      ^ ((uint16_t)data << 3));
}

uint16_t CRC16::sandels(uint16_t crc, uint8_t data){  
  /*
    this function is equivalent to this crc calculator circuit
    the circuit updates the crc by 1 bit of input data per clock pulse

    crc MSB................................................................................................................................crc LSB

    []->xor->[]->xor->[]->xor->[]->xor->[]->xor->[]->xor->[]->xor->[]->xor->[]->xor->[]->xor->[]->xor->[]->xor->[]->xor->[]->xor->[]->xor->[]->xor<-data
    ^    ^        ^        ^        ^        ^        ^        ^        ^        ^        ^        ^        ^        ^        ^        ^        |
    |    |        |        |        |        |        |        |        |        |        |        |        |        |        |        |        |
    |   and      and      and      and      and      and      and      and      and      and      and      and      and      and      and       |
    |   ^ ^      ^ ^      ^ ^      ^ ^      ^ ^      ^ ^      ^ ^      ^ ^      ^ ^      ^ ^      ^ ^      ^ ^      ^ ^      ^ ^      ^ ^       |
    |   | |      | |      | |      | |      | |      | |      | |      | |      | |      | |      | |      | |      | |      | |      | |       |
    ----|-*------|-*------|-*------|-*------|-*------|-*------|-*------|-*------|-*------|-*------|-*------|-*------|-*------|-*------|-*--------
        |        |        |        |        |        |        |        |        |        |        |        |        |        |        |
       [0]      [0]      [0]      [0]      [1]      [0]      [0]      [0]      [0]      [0]      [0]      [1]      [0]      [0]      [0]
  */

  // and gate constant inputs
  const uint16_t xor_enable_mask = 0x0408;

  // loop for each input bit
  for (int bit_counter = 8; bit_counter--; data >>= 1)
  {
    // calculate value of the xor of crc LSB and next input data bit
    uint16_t new_msb = ((uint16_t)data ^ crc) & 1;

    // broadcast new new MSB to all and gates
    uint16_t xor_input_mask = 0 - new_msb;

    // shift crc down by 1 and thrue the xor gates and append new MSB to the crc
    crc = ((crc >> 1) ^ (xor_input_mask & xor_enable_mask)) | (new_msb << 15);
  }
  return crc;
}

void CRC16::generateLookupTable(){
  if(calc_method == LOOKUP_TABLE){
    lookup_table = (uint16_t*)malloc(sizeof(uint16_t) * 256);
  
    uint16_t datum;  
    for(uint16_t i = 0; i < 256; ++i){
      datum = i;
      datum <<= 8;
      lookup_table[i] = calcByte(datum);
    }
  }
}

uint16_t CRC16::calcByte(uint16_t datum){
  bool msb;
	for(uint8_t i = 0; i < 8; ++i){
    msb = datum >> 15 & 1;
    datum <<= 1;
  	if(msb)  	
  		datum ^= poly;
  } return datum;
}

uint8_t CRC16::reflect8(uint8_t reversee){
    reversee = (((reversee & 0xAA) >> 1) | ((reversee & 0x55) << 1));
    reversee = (((reversee & 0xCC) >> 2) | ((reversee & 0x33) << 2));    
    return (reversee >> 4) | (reversee << 4);
}

uint16_t CRC16::reflect16(uint16_t reversee){
    reversee = (((reversee & 0xAAAA) >> 1) | ((reversee & 0x5555) << 1));
    reversee = (((reversee & 0xCCCC) >> 2) | ((reversee & 0x3333) << 2));    
    reversee = (((reversee & 0xF0F0) >> 4) | ((reversee & 0x0F0F) << 4));    
    return (reversee >> 8) | (reversee << 8);
}
