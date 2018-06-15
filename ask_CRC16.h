/* General purpose 16-bit CRC by Jarno Poikonen
   Great place for newbies to get a grasp on Cyclic Reduncancy Checks (CRCs), go read
   http://www.repairfaq.org/filipg/LINK/F_crc_v31.html
   If something is mentioned about CRC model and its convention in this document, it is due to the
   material there. Also just googling up a few online CRC calculators and checking their
   javascript source for implementation, you will quickly see the same CRC model principals
   everywhere. Therefore, this work is merely a port from existing solutions. CRCs are difficult
   to roll by yourself. At least the so-called FAST_CRCs are. */
#ifndef ASK_CRC16_H
#define ASK_CRC16_H
#include <stdint.h>

/* This CRC implementation defines 3 different methods to calculate the 16-bit CRC for an 8-bit input data:
   Those methods are called BITWISE, LOOKUP_TABLE, FAST_CRC.

BITWISE:      Calculates the 16-bit CRC bit by bit for 8-bit input data on every function call.
              Sacrifices processing speed over memory consumption.

LOOKUP_TABLE: Precomputes all the 16-bit CRC combinations for any given 8-bit input byte. (256 combinations)
              This is done in the constructor when the passed in calc_method is defined as LOOKUP_TABLE.
              On function call, calculates the key to access the lookup_table for the actual 16-bit CRC. 
              Sacrifices memory over processing speed.

FAST_CRC:     Combines the good from both BITWISE and LOOKUP_TABLE, is way faster than either and consumes
              no memory for things like lookup.
              FAST_CRC works only on the so called KERMIT algorithm (polynomial: 0x1021) in this implementation.

SANDELS:      Homebrew method made by Santtu Nyman. Marginally faster than BITWISE or LOOKUP_TABLE -methods, 
              consumes no extra memory. Is way slower than FAST_CRC, only works
              as KERMIT algorithm. Don't use SANDELS, since FAST_CRC is a 5x faster. */
enum CALC_METHOD {BITWISE, LOOKUP_TABLE, FAST_CRC, SANDELS};

class CRC16
{
public:
  uint16_t poly;
	uint16_t init;
	uint16_t xorout;
	bool refin;
	bool refout;
	CALC_METHOD calc_method;

  /* Creates a CRC calculator instance.
  name - optional
  poly - required (google up CRC calculator to find lots of polynomials or check the en.wikipedia.org for CRC)
  init - required (the initial value the CRC should have according to the CRC model convention, doesn't apply to FAST_CRC)
  xorout - required (the final value the recent CRC should xor with to get the final CRC)
  refin - required (determines whether every data input should be inverted bitwise before processing further, this is applied on every element for an array of data)
  refout - required (determines whether CRC should be inverted bitwise before xorout operation, this is applied last (only once) for an array of data)
  calc_method - optional (not part of the CRC model convention)
  */

	CRC16(){};
	CRC16(const uint16_t poly, 
        const uint16_t init, 
        const uint16_t xorout, 
        const bool refin, 
        const bool refout, 
        CALC_METHOD calc_method);
  ~CRC16();

  /* Converts the string input into uint8_t input and delegates all the work to an overload.*/
	uint16_t completeLookupCompute(const char* data, uint8_t length);

  /* Computes the final CRC for an array of byte data using the LOOKUP_TABLE approach.
  Attempt to use this function when calc_method is not LOOKUP_TABLE, will only return the
  initial crc value. */
	uint16_t completeLookupCompute(uint8_t* data, uint8_t length);

  /* Computes the next (incomplete) CRC based on given crc and data.
  Requires a call to complete(uint16_t crc) after last input to get the actual CRC.
  Uses the LOOKUP_TABLE approach.
  Attempt to use this function when calc_method is not LOOKUP_TABLE, will only return the
  input crc value unmodified. */
	uint16_t incompleteLookupCompute(uint16_t crc, uint8_t data);

  /* Converts the string input into uint8_t input and delegates all the work to an overload. */
  uint16_t completeBitwiseCompute(const char* data, uint8_t length);

  /* Computes the final CRC for an array of byte data using a BITWISE approach.
  This function is callable regardless of calc_method value. */
  uint16_t completeBitwiseCompute(uint8_t* data, uint8_t length);

  /* Computes the next (incomplete) CRC based on given crc and data.
  Requires a call to complete(uint16_t crc) after last input to get the actual CRC.
  Uses the BITWISE approach. */
	uint16_t incompleteBitwiseCompute(uint16_t crc, uint8_t data);

	uint16_t complete(uint16_t crc);

  uint16_t fastCRC(uint16_t crc, uint8_t data);
  uint16_t sandels(uint16_t crc, uint8_t data);

	private:
  /* Holds the dynamically allocated memory for precomputed 16/bit CRCs,
  when internally accessed with an 8-bit key. */
  uint16_t* lookup_table;

  // Conditionally generates the lookup table in constructor, if calc_method is defined as LOOKUP_TABLE.
	void generateLookupTable();

  /* Internally bitwise calculates 16-bit CRCs for a width of 8 bits (1 byte).
     Either on every function call when using BITWISE -method or
     once for every 8-bit key combination during lookup table generation when using the LOOKUP_TABLE -method. */
	uint16_t calcByte(uint16_t data);

  /* Some CRC model algorithms such as KERMIT do require that every input byte must be reflected, meaning that
  the bits of the byte must be inverted before any further processing. */
	uint8_t reflect8(uint8_t reversee);

  /* Some CRC model algorithms such as KERMIT do require that reflection to the final output is to be applied, i.e.
  reversing the bits of the 16-bit CRC after processing the final input. */
	uint16_t reflect16(uint16_t reversee);
};

#endif