/* General purpose 16-bit CRC by Jarno Poikonen
   Great place for newbies to get a grasp on Cyclic Reduncancy Checks (CRCs), go read
   http://www.repairfaq.org/filipg/LINK/F_crc_v31.html
   If I mention something about CRC model and its convention in this document, it is due to the
   material there. Also just googling up a few online CRC calculators and checking their
   javascript source for implementation, you will quickly see the same CRC model principals
   every where. Therefore, this work is merely a port from existing solutions. CRCs are difficult
   to roll by yourself. At least efficient CRCs are. */
#ifndef CRC16_H
#define CRC16_H
#include <stdint.h>

/* There are 3 definitions on different methods to calculate a CRC:
1) BITWISE: The calculation of CRC, bit by bit for a full 8-bit byte, on EVERY function call.

2) LOOKUP_TABLE: Precomputing all CRC values into a lookup table, and instead of calculating
the CRC on every function call, it is just fetched from a lookup table. Using a lookup table
consumes additional memory to hold the fetchable, precomputed values during runtime.

3) FAST_CRC(for KERMIT algorithm only): An optimized method for one specific algorithm which is
WAY FASTER than BITWISE or LOOKUP_TABLE -methods. Excessive memory consumption free. FAST_CRC 
is the preferred calculation method, however finding or deriving such a method yourself - for
more algorithms - requires extensive knowledge in CRC calculation, math and optimization 
techniques.

This header file knows only one such method, and was directly copied as is from the
radiohead RHCRC.H/RHCRC.CPP files for "Amplitude Shift Keying" transmitter/reveicer.

This method inherently follows the model of the KERMIT algorithm except since it is optimized, it
doesn't follow the convention of the CRC model to the letter, but produces the same results for all
inputs as if it were using the KERMIT algorithm of the CRC model.

An example on the quirkiness of FAST_CRC compared to the CRC MODEL:
Instead of starting with an init value of 0x0000 as the model for KERMIT algorithm would dictate,
the FAST_CRC equivalent actually starts with 0xFFFF as the init value, this breaks the convention
of the CRC model, but it yields the same results in the end. I suppose it is safe to assume that all
FAST_CRCs algorithms differ in some regard compared to the model.

So in case you are using the KERMIT algorithm, use FAST_CRC, otherwise use either BITWISE or
LOOKUP_TABLE.

In case you are using the next best thing due to not being able to use a FAST_CRC on your chosen algorithm, 
use LOOKUP_TABLE -method, it is way faster than BITWISE, except it will consume 256*16 bits of memory
for the lookup table.

In case you wish to save memory and are willing to sacrifice cycles, then use BITWISE -method.

Ideally it would be great to have a FAST_CRC implementation for every conceivable CRC model algorithm,
and integrate them all into this header file (for 16 bit polynomials that is), but finding those,
is not a priority at this time. Feel free to add some.

4) SANDELS(for KERMIT algorithm only): Follows no model, is a homebrew method made by Santtu Nyman. Marginally faster than
BITWISE or LOOKUP_TABLE -methods, consumes no extra memory. Is way slower than FAST_CRC, only works
as KERMIT algorithm. Don't use SANDELS, since FAST_CRC is a 5x faster. 
*/
enum CALC_METHOD {BITWISE, LOOKUP_TABLE, FAST_CRC, SANDELS};

class CRC16
{
public:
  const char* name;
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
	CRC16(const char* name,
        const uint16_t poly, 
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