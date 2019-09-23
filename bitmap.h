#pragma once
#include <stdint.h>
#include "error.h"
typedef struct{
	int num_bit; //number of bits
	char* entries; //number of entries
} BitMap;


typedef struct{
	int entry_num; //number of entry
	char bit_num; //offset
} BitMapEntryKey;

#define BIT_NUM 8 //bit in a byte
#define mask 0x01 //mask for one bit (00000001)

//returns the value of the bit in position "pos"
int BitMap_getBit(BitMap* bmp, int pos);

//converts a block index to an index in the array
//and a char that stands for the offset of the bit inside the array
BitMapEntryKey BitMap_blockToIndex(int num);

//converts a bit to a linear index, returns -1 if any error occurs
int BitMap_indexToBlock(int entry, char bit_num);

//returns the index of the first bit having "status" status
//(0 or 1), starts from position "start"
int BitMap_get(BitMap* bmp, int start, int status);

//sets the bit at index pos in bmp to status
int BitMap_set(BitMap* bmp, int pos, int status);




