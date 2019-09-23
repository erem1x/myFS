#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "bitmap.h"

int BitMap_getBit(BitMap* bmp, int pos){
	if(pos>=bmp->num_bit) return -1; //out of range
	BitMapEntryKey map=BitMap_blockToIndex(pos); //easier to shift
	//shifting the right byte "offset-times",
	//applying the mask to get the right bit (last bit now)
	return bmp->entries[map.entry_num] >> map.bit_num & mask;
}

BitMapEntryKey BitMap_blockToIndex(int num){
	//simply uses math to get the right index out of a block
	BitMapEntryKey map;
	int byte= num / BIT_NUM;
	map.entry_num=byte;
	char offset=num-(byte*BIT_NUM);
	map.bit_num=offset;
	return map;
}

int BitMap_indexToBlock(int entry, char bit_num){
	if (entry<0 || bit_num<0) return -1; //block not in range
	return (entry*BIT_NUM)+bit_num;
}

int BitMap_get(BitMap* bmp, int start, int status){
	if(start>bmp->num_bit) return -1; //start not in range
	while(start<bmp->num_bit){ //looping to find status
		if(BitMap_getBit(bmp, start)==status) return start;
		start++;
	}
	return -1;
}

int BitMap_set(BitMap* bmp, int pos, int status){
	if(pos>=bmp->num_bit) return -1; //out of range
	
	BitMapEntryKey map=BitMap_blockToIndex(pos);
	unsigned char changer=1 << map.bit_num; //mask for the bit in the byte
	unsigned char to_be_changed=bmp->entries[map.entry_num]; //byte to apply the mask	
	if(status==1){
		bmp->entries[map.entry_num] = to_be_changed | changer; //mask  through OR to get 1
		return to_be_changed | changer;
	}
	
	else{
		bmp->entries[map.entry_num]=to_be_changed & (~changer); //mask thorugh AND to get 0 (using !mask)
		return to_be_changed & (~changer);
	}
}

		
	
	
	
