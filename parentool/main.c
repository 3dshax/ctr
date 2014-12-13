#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

unsigned int calculate_master_key(unsigned char* generator)
{
	uint32_t table[0x100];
	uint32_t data;
	uint32_t i, j;
	uint32_t y;
	uint8_t x;
	uint64_t yll;
	uint32_t yhi;

	for(i=0; i<0x100; i++)
	{
		data = i;
		for(j=0; j<4; j++)
		{
			if (data & 1)
				data = 0xEDBA6320 ^ (data>>1);
			else
				data = data>>1;

			if (data & 1)
				data = 0xEDBA6320 ^ (data>>1);
			else
				data = data>>1;
		}

		table[i] = data;
	}

	y = 0xFFFFFFFF;
	x = generator[0];
	for(i=0; i<4; i++)
	{
		x ^= y;
		y = table[x] ^ (y>>8);
		x = generator[1+i*2] ^ y;
		y = table[x] ^ (y>>8);
		x = generator[2+i*2];
	}

	y ^= 0xAAAA;
	y += 0x1657;

	yll = y;
	yll = (yll+1) * 0xA7C5AC47ULL;
	yhi = (yll>>48);
	yhi *= 0xFFFFF3CB;
	y += (yhi<<5);

	return y;
}

int main(int argc, const char* argv[])
{
	unsigned char generator[9] = {0};
	unsigned int servicecode, month, day, masterkey;

	if (argc != 4)
	{
		printf("usage: <servicecode> <month> <day>\n");
		exit(1);
	}

	servicecode = strtoul(argv[1], 0, 10);
	month = strtoul(argv[2], 0, 10);
	day = strtoul(argv[3], 0, 10);
	
	servicecode %= 10000;
	month %= 100;
	day %= 100;
	sprintf((char*)generator, "%02d%02d%04d", month, day, servicecode);

	masterkey = calculate_master_key(generator);

	printf("Master key is %05d\n", masterkey);
	return 0;
}
