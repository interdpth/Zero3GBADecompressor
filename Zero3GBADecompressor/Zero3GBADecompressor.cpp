// thething.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

struct CompressedObject {
	unsigned long screen;
	unsigned short roomDataInfo;
	unsigned short chunksize;
	unsigned short layersize;
	unsigned short roombitfield;
};


#define RegularLayer      1
#define LZCompressed     2
#define RLCompressed    0x1000
#define u32 unsigned long
#define s32 signed long

static void CpuSet(unsigned long* source, unsigned long* dest, unsigned long mode) 
{
	unsigned short* src16 = (unsigned short*)source; 
    unsigned short* dst16 = (unsigned short*)dest; 

	int count = mode & 0x000FFFFF;
	int fill = mode & 0x01000000;
	int wordsize = (mode & 0x04000000) ? 4 : 2;
	int i;
	if (fill) {
		if (wordsize == 4) 
		{		
			unsigned long word = source[0];
			for (i = 0; i < count; ++i) 
			{
				dest[(i << 2)] = word;				
			}
		}
		else 
		{
			unsigned long word = src16[0];
			for (i = 0; i < count; ++i) 
			{
				dst16[(i << 2)] = word;
			}
		}
	}
	else 
	{
		if (wordsize == 4) 
		{		
			for (i = 0; i < count; ++i) 
			{
				dest[(i << 2)] = source[(i << 2)];
			}
		}
		else 
		{			
			for (i = 0; i < count; ++i) 
			{			
				dst16[(i << 1)] = src16[(i << 1)];
			}	
		}
	}
}

int LZUncomp(unsigned char * source, unsigned char * dest)
{
	long            size = 0;

	unsigned long             header = (source[0] | (source[1] << 8) | (source[2] << 16) | (source[3] << 24));

	
	int             i, j, in = 4, out = 0;
	int             length, offset;
	unsigned long             windowOffset;

	int             len = header >> 8;

	while (len > 0)
	{
		unsigned char              d = source[in++];

		for (i = 0; i < 8; i++)
		{
			if (d & 0x80)
			{
				unsigned short             data = ((source[in] << 8) | source[in + 1]);

				in += 2;
				length = (data >> 12) + 3;
				offset = (data & 0x0FFF);
				windowOffset = out - offset - 1;
				for (j = 0; j < length; j++)
				{
					dest[out++] = dest[windowOffset++];
					len--;
					if (len == 0)
						return header >> 8;
				}
			}
			else
			{
				dest[out++] = source[in++];
				len--;
				if (len == 0)
					return header >> 8;
			}
			d <<= 1;
		}
	}
	return header >> 8;
}


static void FastCpuSet( unsigned long* source, unsigned long* dest, unsigned long mode)
{
	int count = mode & 0x000FFFFF;
	int i;
	if (mode & 0x01000000) {
		unsigned long word = source[0];
		for (i = 0; i < count; i += 4) {
			dest[((i + 0))] = word;
			dest[((i + 1))] = word;
			dest[((i + 2))] = word;
			dest[((i + 3))] = word;
		}
	}
	else
	{
		for (i = 0; i < count; i += 4)
		{
			unsigned long word0 = source[((i + 0))];
			unsigned long word1 = source[((i + 1) )];
			unsigned long word2 = source[((i + 2) )];
			unsigned long word3 = source[((i + 3) )];

			dest[((i + 0))] = word0;
			dest[((i + 1))] = word1;
			dest[((i + 2))] = word1;
			dest[((i + 3))] = word1;
		}
	}
}


static void FastCpuSet(unsigned char* source, unsigned char* dest, unsigned long mode)
{
	FastCpuSet( (unsigned long*)source, (unsigned long*)dest, mode);
}
static void RlUncomp(unsigned long* source, unsigned char* dest) {

	int remaining = (source[0] &0xFFFFFF00) >> 8;
	int padding = (4 - remaining) & 0x3;
	// We assume the signature unsigned char (0x30) is correct
	int blockheader;
	int block;
	unsigned long* sPointer = source + 4;
	unsigned char* dPointer = dest;
	while (remaining > 0) {
		blockheader = (int)((unsigned char*)sPointer++);
		if (blockheader & 0x80) {
			// Compressed
			blockheader &= 0x7F;
			blockheader += 3;
			block = (int)((unsigned char*)sPointer++);
			while (blockheader-- && remaining) {
				--remaining;
				*dPointer = block;
				++dPointer;
			}
		}
		else {
			// Uncompressed
			blockheader++;
			while (blockheader-- && remaining) {
				--remaining;
				*dPointer = (int)((unsigned char*)sPointer++);
				++dPointer;
			}
		}
	}
	while (padding--) {
		*dPointer = 0;
		++dPointer;
	}
}
unsigned char * buffer;
int CopyRoomDataToScreen(CompressedObject *datapointer, int memstart)
{
	
	CompressedObject *roomPointer; 
	int storedSize; 
	unsigned int dest; 
	unsigned short *currentLayer; 
	int copySize;
	int nextSize; 
	int layerSize; 
	int sz; 
	unsigned char headerStorage[8]; 
	int v12; 

	roomPointer = datapointer;
	storedSize = *&datapointer->roomDataInfo & 0x3FFFFF;
	if (storedSize)
	{
		dest = memstart + (datapointer->roombitfield & 0x7F8) * (datapointer->chunksize >> 6);
		if (datapointer->roombitfield & RegularLayer)
		{
			currentLayer = (unsigned short*)(datapointer + datapointer->screen);
			copySize = *&datapointer->roomDataInfo & 0x3FFFFF;
			if (datapointer->roombitfield & LZCompressed)
			{
				if (storedSize > 0)
				{
					do
					{
						if (*currentLayer >= 0)
						{
							FastCpuSet((unsigned long*)(currentLayer + 2), (unsigned long*)(dest + buffer), roomPointer->layersize >> 2);
						}
						else
						{
							FastCpuSet((unsigned long*)(dest + roomPointer->layersize + buffer),(unsigned long*) &headerStorage, 8);
							LZUncomp((unsigned char*)(currentLayer + 2), dest + buffer);
							FastCpuSet((unsigned long*)&headerStorage, (unsigned long*)(dest + roomPointer->layersize + buffer), 8);
						}
						dest += roomPointer->roombitfield << 21 >> 24 << 8;
						nextSize = *currentLayer & 0x7FFFFFFF;
						copySize -= nextSize;
						currentLayer = (currentLayer + nextSize);
					} while (copySize > 0);
				}
			}
			else if (datapointer->roombitfield & RLCompressed)
			{
				if (storedSize > 0)
				{
					do
					{
						if (*currentLayer >= 0)
							FastCpuSet((unsigned long*)(currentLayer + 2), (unsigned long*)(dest + buffer), roomPointer->layersize  >> 2);
						else
							RlUncomp((unsigned long*)(currentLayer + 2), (unsigned char*)(dest + buffer));
						dest += roomPointer->roombitfield << 21 >> 24 << 8;
						layerSize = *currentLayer & 0x7FFFFFFF;
						copySize -= layerSize;
						currentLayer = (currentLayer + layerSize);
					} while (copySize > 0);
				}
			}
			else if (storedSize > 0)
			{
				do
				{
					FastCpuSet((unsigned long*)currentLayer, (unsigned long*)(dest + buffer), roomPointer->layersize >> 2);
					dest += roomPointer->roombitfield << 21 >> 24 << 8;
					sz = roomPointer->layersize;
					currentLayer = (currentLayer + sz);
					copySize -= sz;
				} while (copySize > 0);
			}
		}
		else if (datapointer->roombitfield & LZCompressed)
		{
			int len = LZUncomp((unsigned char*)(datapointer + datapointer->screen), (dest + buffer));
			printf("len %x", len);
		}
		else if (datapointer->roombitfield & RLCompressed)
		{
			RlUncomp((unsigned long*)(datapointer + datapointer->screen), (dest + buffer));
		}
		else
		{
			FastCpuSet((unsigned char*)((unsigned char*)datapointer + datapointer->screen), (unsigned char*)(dest + buffer), storedSize << 2);
		}
	}


	return 0;
}

int main()
{
	buffer = new unsigned char[0x3FFFF];
	memset(buffer, 0, 0x3FFFF);
	FILE* fp = NULL;
	fopen_s(&fp, "z3.gba", "r+b");
	fseek(fp, 0, SEEK_END);
	int sz = ftell(fp);
	unsigned char* file = new unsigned char[sz];
	fseek(fp, 0, SEEK_SET);

	fread(file, 1, sz, fp);
	fclose(fp);

	//Test usages
	CompressedObject* rd = (CompressedObject*)&file[0x7d5b80];
	CopyRoomDataToScreen( rd, 0x4000);

	//Examples
	/*rd = (room*)&file[0x7d5b1c];
	CopyRoomDataToScreen(rd, 0x4000);

	rd = (room*)&file[0x7d5b6c];
	CopyRoomDataToScreen(rd, 0x4000);
	rd = (room*)&file[0x7d5b44];
	CopyRoomDataToScreen(rd, 0x4000);
	rd = (room*)&file[0x7d5b30];
	CopyRoomDataToScreen(rd, 0x4000);
	rd = (room*)&file[0x7d5b58];
	CopyRoomDataToScreen(rd, 0x4000);

	rd = (room*)&file[0x7d5AE0];
	CopyRoomDataToScreen(rd, 0x4000);

	rd = (room*)&file[0x7d5b6c];
	CopyRoomDataToScreen(rd, 0x4000);
*/
	/*rd = (room*)&file[0x3371e4];
	CopyRoomDataToScreen(rd, 0x4000);*/
	rd = (CompressedObject*)&file[0x7d5b58];
	CopyRoomDataToScreen(rd, 0x4000);
	//rd = (room*)&file[0x7d5a54];
	//CopyRoomDataToScreen(rd, 0x4000);
	//rd = (room*)&file[0x7d5a2c];
	//CopyRoomDataToScreen(rd, 0x4000);
	//
	//rd = (room*)&file[0x5473d4];
	//CopyRoomDataToScreen(rd, 0x8000);

	//rd = (room*)&file[0x547348];
	//CopyRoomDataToScreen(rd, 0x8000);

	//rd = (room*)&file[0x3b8fb8];
	//CopyRoomDataToScreen(rd, 0x10000);

	delete[] file;

	fopen_s(&fp, "test.gba", "w+b");
	fwrite(buffer, 1, 0x3FFFF, fp);
	fclose(fp);
	delete[] buffer;
    return 0;
}

