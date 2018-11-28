/*
 * Copyright (C) 2018 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#define __USE_LARGEFILE64
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <malloc.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <zstd.h>
#include "crc32.h"

// 16MB i/o buf
#define IOBUFLEN 16*1024*1024

// main file header
typedef struct __attribute__((__packed__)) _arc_hdr1 {
	uint64_t magic; // 10 32 54 76 98 EF CD AB
	uint64_t hdrsize; // 0x38
	int64_t fil1_off; // first offset shift
	int64_t fil2_off;
	int64_t hdr2_off; // first big header
	int64_t hdr3_off; // second big header
	uint64_t empty;
} arc_hdr1;

//at hdr2_off
typedef struct __attribute__((__packed__)) _arc_hdr2 {
	uint32_t hdrlen;
	// handles file structure after 2gb, can be compressed
	uint32_t struct6_11_enum; //0x64EE
	uint32_t struct7_enum_part1; //0x8DE9, first half of 0x8F83
	uint32_t struct9_14_enum1; //0x73BBA
	uint32_t struct10_enum_part1; //0x860D1, first half of 0x89B11
	uint32_t struct13_enum; //0x73376
	uint32_t struct8_enum; //0x64D1
	uint32_t struct9_14_enum2; //0x73BBA
	uint32_t struct7_enum_part2; //0x19A, second half of 0x8F83
	uint32_t struct10_enum_part2; //0x3A40, second half of 0x89B11
	uint32_t sumenum5;
	uint32_t somenum6;
	uint32_t somenum7;
	uint32_t somenum8;
	// handles file structure of first 2gb, streamed music/video
	uint32_t struct1_2_enum; //0x980
	uint32_t struct3_enum; //0x1501
	uint32_t struct4_enum; //0xAA1
	uint32_t bgmhash; //crc32 of "bgm"
	uint8_t bgmhashlen; //3
	uint8_t bgm_struct2_entries[3]; //0x818
	uint32_t bgm_struct2_entry_start; //0
	uint32_t appealhash; //crc32 of "smashappeal"
	uint8_t appealhashlen; //0xB
	uint8_t appeal_struct2_entries[3]; //0x112
	uint32_t appeal_struct2_entry_start; //0x818
	uint32_t moviehash; //crc32 of "movie"
	uint8_t moviehashlen; //5
	uint8_t movie_struct2_entries[3]; //0x56
	uint32_t movie_struct2_start; //0x92A
} arc_hdr2;

// the first 2gb are handled by the following 4 structs

// at hdr2_off+0x68, 0x4C00 bytes, 0x980 entries, first hash table sorted by name hash length value
typedef struct __attribute__((__packed__)) _hdr2_struct1 {
	uint32_t fullpathhash;
	uint8_t fullpathhashlen;
	uint8_t struct2_enum[3];
} hdr2_struct1;

// at hdr2_off+0x4C68, 0x7200 bytes, 0x980 entries, second hash table sorts by increasing index
typedef struct __attribute__((__packed__)) _hdr2_struct2 {
	uint32_t fullpathhash;
	uint8_t fullpathhashlen;
	//this index increases by more than 1 depending
	//on what the numfiles value is below
	uint8_t struct3_enum[3];
	//00=1 entry for struct 3
	//01=0xE entries for struct 3
	//02=0x5 entries for struct 3
	uint32_t numfiles;
} hdr2_struct2;

// at hdr2_off+0xBE68, 0x5404 bytes, 0x1501 entries, file id table, to point to exact file number below
typedef struct __attribute__((__packed__)) _hdr2_struct3 {
	uint32_t struct4_enum;
} hdr2_struct3;

// at hdr2_off+0x1126C, 0xAA10 bytes, file offset table indexed with table above
typedef struct __attribute__((__packed__)) _hdr2_struct4 {
	uint32_t len;
	uint32_t empty;
	uint64_t off;
} hdr2_struct4;

// TODO: figure out all struct uses below
// at hdr2_off+0x1BC7C, 0xA8 bytes, 0xE elements
typedef struct __attribute__((__packed__)) _hdr2_struct5 {
	uint32_t unk1;
	uint32_t unk2;
	uint32_t unk3;
} hdr2_struct5;

// at hdr2_off+0x1BD24, 0x148058 bytes, 0x64EE elements, for base groups?
typedef struct __attribute__((__packed__)) _hdr2_struct6 {
	uint32_t fullgrouphash; //for example "param/sound"
	uint8_t fullgrouphashlen;
	uint8_t struct7_enum[3]; //needed to get full file offset
	uint32_t groupsubhash; //for example "sound"
	uint8_t groupsubhashlen;
	uint8_t unk1[3];
	uint32_t groupbasehash; //for example "param"
	uint8_t groupbasehashlen;
	uint8_t unk2[3];
	uint32_t unk3;
	uint32_t unk4;
	uint32_t someindex;
	uint32_t unk5;
	uint32_t unk6;
	uint32_t unk7;
	uint32_t unk8;
} hdr2_struct6;

// at hdr2_off+0x163D7C, 0xFB254 bytes, 0x8F83 elements
typedef struct __attribute__((__packed__)) _hdr2_struct7 {
	//not quite all, also have to add fil1_off from arc_hdr1
	//also offsets+size seem to go all the way up to hdr2_off
	uint64_t base_offset;
	uint32_t unk1;
	//seems to be set right before next base offset
	uint32_t base_size;
	uint32_t unk2;
	uint32_t unk3;
	uint32_t unk4;
} hdr2_struct7;

// at hdr2_off+0x25EFD0, 0x32688 bytes, 0x64D1 entries, name single hashes
typedef struct __attribute__((__packed__)) _hdr2_struct8 {
	uint32_t fullpathhash;
	uint8_t fullpathhashlen;
	uint8_t unk[3];
} hdr2_struct8;

// at hdr2_off+0x291658, 0x1215510 bytes, 0x73BBA entries, name multi hashes
typedef struct __attribute__((__packed__)) _hdr2_struct9 {
	uint32_t fullpathhash; //full path plus name plus extension
	uint8_t fullpathhashlen;
	uint8_t struct6_11_enum[3];
	uint32_t extensionhash; //extension after the .
	uint8_t extensionhashlen;
	uint8_t unk1[3];
	uint32_t pathhash; //path before filename WITH trailing /
	uint8_t pathhashlen;
	uint8_t unk2[3];
	uint32_t filehash; //filename after trailing /
	uint8_t filenhashlen;
	uint8_t unk3[3];
	uint32_t struct10_enum;
	uint32_t unk4;
} hdr2_struct9;

// at hdr2_off+0x14A6B68, 0x89B110 bytes, 0x89B11 entries, lists every file (?)
typedef struct __attribute__((__packed__)) _hdr2_struct10 {
	// multiple local offset by 4 and add on top of struct 7 offset to get full file offset
	uint32_t file_local_offset;
	uint32_t file_len_cmp;
	uint32_t file_len_decmp;
	uint32_t flags; //various flags, 0x03000000 indicates zstd compression
} hdr2_struct10;

// at hdr2_off+0x1D41C78, 0x32770 bytes, 0x64EE entries
typedef struct __attribute__((__packed__)) _hdr2_struct11 {
	uint32_t somehash;
	uint8_t somehashlen;
	//every entry just adds 1 to this
	uint8_t struct6_11_enum[3];
} hdr2_struct11;

// at hdr2_off+0x1D743E8, 8 bytes
typedef struct __attribute__((__packed__)) _hdr2_struct12_hdr {
	uint32_t totalnums; //0x73376
	uint32_t struct11_elements; //0x400
} hdr2_struct12_hdr;

// at hdr2_off+0x1D743F0, 0x2000 bytes, 0x400 entries
typedef struct __attribute__((__packed__)) _hdr2_struct12 {
	uint32_t somenum; //counts from 0 up to 0x73376
	uint32_t someadd; //value added each entry
} hdr2_struct12;

// at hdr2_off+0x1D763F0, 0x399BB0 bytes, 0x73376 entries
typedef struct __attribute__((__packed__)) _hdr2_struct13 {
	uint32_t fullpathhash;
	uint8_t fullpathhashlen;
	uint8_t someindex[3]; //possibly similar index to struct 6
} hdr2_struct13;

// at hdr2_off+0x210FFA0, 0x39DDD0 bytes, 0x73BBA entries
typedef struct __attribute__((__packed__)) _hdr2_struct14 {
	uint8_t empty[5];
	//every entry just adds 1 to this
	uint8_t num[3];
} hdr2_struct14;

// done, all 0x24ADD70 bytes of hdr2 processed,
// hdr3 follows right behind it, TODO: look at hdr3 in more detail
typedef struct __attribute__((__packed__)) _arc_hdr3 {
	uint32_t hdrlen;
	uint32_t empty;
	// also used for files past 2gb in some way I think
	uint32_t struct1_2_enum; //0x7541
	uint32_t struct3_4_5_enum1; //0x80272
	uint32_t struct3_4_5_enum2; //0x80272
} arc_hdr3;

//at hdr3_off+0x14, 0x3AA08 bytes, 0x7541 entries
typedef struct __attribute__((__packed__)) _hdr3_struct1 {
	uint32_t somehash;
	uint8_t somehashlen;
	uint8_t unk1[3];
} hdr3_struct1;

//at hdr3_off+0x3AA1C, 0xEA820 bytes, 0x7541 entries
typedef struct __attribute__((__packed__)) _hdr3_struct2 {
	uint32_t somehash1;
	uint8_t somehashlen1;
	uint8_t unk1[3];
	uint32_t somehash2;
	uint8_t somehashlen2;
	uint8_t unk2[3];
	uint32_t somehash3;
	uint8_t somehashlen3;
	uint8_t unk3[3];
	uint32_t unk4;
	uint32_t unk5;
} hdr3_struct2;

//at hdr3_off+0x12523C, 0x401390 bytes, 0x80272 entries, though first 0x729D are blank!
typedef struct __attribute__((__packed__)) _hdr3_struct3 {
	uint32_t somehash;
	uint8_t somehashlen;
	uint8_t unk1[3];
} hdr3_struct3;

//at hdr_off+0x5265CC, 0x1E3F54 bytes, 0x80272 entries
typedef struct __attribute__((__packed__)) _hdr3_struct4 {
	uint32_t num; //just counts up
} hdr3_struct4;

// at hdr3_off+0x726F94, 0xF1FAA0 bytes, 0x80272 entries, name multi hashes
typedef struct __attribute__((__packed__)) _hdr3_struct5 {
	uint32_t fullpathhash; //full path plus name plus extension
	uint8_t fullpathhashlen;
	uint8_t unk1[3];
	uint32_t pathhash; //path before filename WITHOUT trailing /
	uint8_t pathhashlen;
	uint8_t unk2[3];
	uint32_t filehash; //filename after trailing /
	uint8_t filenhashlen;
	uint8_t unk3[3];
	uint32_t extensionhash; //extension after the .
	uint8_t extensionhashlen;
	uint8_t unk4[3];
} hdr3_struct5;

// done, end of hdr3, end of file

//TOC of NUS3 PACK
typedef struct __attribute__((__packed__)) _nustoc {
	uint32_t off;
	uint32_t len;
} nustoc;

#define AUDIOMAXF 16
static uint32_t name_offset[AUDIOMAXF];
static nustoc audio_offset[AUDIOMAXF];
#define FILENAMELEN 256
static char audio_name[FILENAMELEN];
static char filewritename[FILENAMELEN];

//all the magic words to check for

static const uint8_t streamname[8] = { 0x73, 0x74, 0x72, 0x65, 0x61, 0x6D, 0x3A, 0x2F };  //stream:/

//audio stuff
static const uint8_t cmpaudwant[8] = { 0x41, 0x55, 0x44, 0x49, 0x49, 0x4E, 0x44, 0x58 }; //AUDIINDX
static const uint8_t cmpaudskip[8] = { 0x42, 0x41, 0x4E, 0x4B, 0x54, 0x4F, 0x43, 0x20 }; //BANKTOC

static const uint8_t tnid[4] = { 0x54, 0x4E, 0x49, 0x44 }; //TNID
static const uint8_t nmof[4] = { 0x4E, 0x4D, 0x4F, 0x46 }; //NMOF
static const uint8_t adof[4] = { 0x41, 0x44, 0x4F, 0x46 }; //ADOF
static const uint8_t tnnm[4] = { 0x54, 0x4E, 0x4E, 0x4D }; //TNNM
static const uint8_t junk[4] = { 0x4A, 0x55, 0x4E, 0x4B }; //JUNK
static const uint8_t pack[4] = { 0x50, 0x41, 0x43, 0x4B }; //PACK
static const uint8_t opus[4] = { 0x4F, 0x50, 0x55, 0x53 }; //OPUS

//video stuff
static const uint8_t cmpvidwant[4] = { 0x77, 0x65, 0x62, 0x6D }; //webm

//buffered write to file
static void writefile(FILE *in, FILE *out, uint8_t *buf, uint32_t len)
{
	while(len)
	{
		size_t proc = (len > IOBUFLEN) ? IOBUFLEN : len;
		fread(buf, 1, proc, in);
		fwrite(buf, 1, proc, out);
		len -= proc;
	}
}

static void write_found_file(FILE *in, uint8_t *buf, const char *inname, uint64_t offset, uint32_t size, bool is_cmp, uint32_t decmp_size)
{
	printf("Absolute File Offset: %08x%08x, File Size: %08x\n",	(uint32_t)(offset>>32), (uint32_t)(offset&0xFFFFFFFF), size);
	if(!offset || !size)
	{
		puts("No offset or length, exit");
		return;
	}
	if(strrchr(inname,'/') != NULL && strrchr(inname,'/')+1 != '\0') inname = strrchr(inname,'/')+1;
	if(strrchr(inname,'\\') != NULL && strrchr(inname,'\\')+1 != '\0') inname = strrchr(inname,'\\')+1;
	//seek to absolute offset
	fseeko64(in, offset, SEEK_SET);
	if(is_cmp)
	{
		printf("Allocating %08x for compressed and %08x bytes for decompressed file\n", size, decmp_size);
		void *decmp_buf = malloc(decmp_size);
		if(decmp_buf)
		{
			void *cmp_buf = malloc(size);
			if(cmp_buf)
			{
				fread(cmp_buf, 1, size, in);
				if(ZSTD_decompress(decmp_buf, decmp_size, cmp_buf, size) != decmp_size)
					puts("Failed to decompress buffer!");
				else
				{
					FILE *wf = fopen(inname, "wb");
					if(wf)
					{
						//write decompressed buffer
						fwrite(decmp_buf, 1, decmp_size, wf);
						fclose(wf);
						printf("File %s decompressed and written successfully!\n", inname);
					}
					else
						printf("Failed writing decompressed file! Unable to open %s!\n", inname);
					
				}
				free(cmp_buf);
			}
			else
				puts("Failed to allocate cmp buf!");
			free(decmp_buf);
		}
		else
			puts("Failed to allocate decmp buf!");
	}
	else
	{
		puts("Writing file directly");
		FILE *wf = fopen(inname, "wb");
		if(wf)
		{
			//seek to absolute offset
			fseeko64(in, offset, SEEK_SET);
			//write file directly
			writefile(in, wf, buf, size);
			fclose(wf);
			printf("File %s written successfully!\n", inname);
		}
		else
			printf("Failed writing file! Unable to open %s!\n", inname);
	}
}

int main(int argc, char *argv[])
{
	puts("Smash Ultimate ARC Extract WIP-2 by FIX94");
	FILE *data = fopen("data.arc", "rb");
	if(!data)
	{
		puts("No data.arc in current directory!");
		return 0;
	}
	puts("Checking data.arc");
	//quick sanity check
	arc_hdr1 hdr1;
	memset(&hdr1, 0, sizeof(arc_hdr1));
	fread(&hdr1, 1, sizeof(arc_hdr1), data);
	if(hdr1.hdrsize != 0x38 || hdr1.hdr2_off == 0)
	{
		puts("data.arc header 1 incorrect!");
		fclose(data);
		return 0;
	}
	//get to our file header
	fseeko64(data, hdr1.hdr2_off, SEEK_SET);
	if(ftello64(data) != hdr1.hdr2_off)
	{
		puts("data.arc seems to be too small!");
		fclose(data);
		return 0;
	}
	//verify file is big enough
	arc_hdr2 hdr2;
	memset(&hdr2, 0, sizeof(arc_hdr2));
	fread(&hdr2, 1, sizeof(arc_hdr2), data);
	if(hdr2.hdrlen == 0)
	{
		puts("data.arc header 2 size missing!");
		fclose(data);
		return 0;
	}
	fseeko64(data, hdr1.hdr2_off+hdr2.hdrlen, SEEK_SET);
	if(ftello64(data) != hdr1.hdr2_off+hdr2.hdrlen)
	{
		puts("data.arc seems to be too small!");
		fclose(data);
		return 0;
	}
	void *iobuf = malloc(IOBUFLEN);
	//all used struct lengths
	uint32_t hdr2_struct1_len = sizeof(hdr2_struct1)*hdr2.struct1_2_enum;
	uint32_t hdr2_struct2_len = sizeof(hdr2_struct2)*hdr2.struct1_2_enum;
	uint32_t hdr2_struct3_len = sizeof(hdr2_struct3)*hdr2.struct3_enum;
	uint32_t hdr2_struct4_len = sizeof(hdr2_struct4)*hdr2.struct4_enum;
	uint32_t hdr2_struct5_len = sizeof(hdr2_struct5)*0xE; //not sure where that number is from yet..
	uint32_t hdr2_struct6_len = sizeof(hdr2_struct6)*hdr2.struct6_11_enum;
	uint32_t hdr2_struct7_len = sizeof(hdr2_struct7)*(hdr2.struct7_enum_part1+hdr2.struct7_enum_part2);
	uint32_t hdr2_struct8_len = sizeof(hdr2_struct8)*hdr2.struct8_enum;
	uint32_t hdr2_struct9_len = sizeof(hdr2_struct9)*hdr2.struct9_14_enum1;
	uint32_t hdr2_struct10_len = sizeof(hdr2_struct10)*(hdr2.struct10_enum_part1+hdr2.struct10_enum_part2);
	//no single filename given, extract all stream:// files
	if(argc < 2)
	{
		puts("No filename given, extracting lopus/webm files");
		//make sure output folders exist
		mkdir("lopus");
		mkdir("webm");
		//skip all the way to struct 4, the offset table
		fseeko64(data, hdr1.hdr2_off+sizeof(arc_hdr2), SEEK_SET);
		fseeko64(data, hdr2_struct1_len, SEEK_CUR);
		fseeko64(data, hdr2_struct2_len, SEEK_CUR);
		fseeko64(data, hdr2_struct3_len, SEEK_CUR);
		//read struct 4
		printf("Reading struct 4 (struct len %08x, total %08x bytes)\n", sizeof(hdr2_struct4), hdr2_struct4_len);
		void *raw_struct4_buf = malloc(hdr2_struct4_len);
		fread(raw_struct4_buf, 1, hdr2_struct4_len, data);
		hdr2_struct4 *struct4_ptr = (hdr2_struct4*)raw_struct4_buf;
		uint16_t sometocnum;
		uint16_t webmcnt = 0;
		uint8_t magicchk[8];
		puts("Extracting all stream:// files into .lopus/.webm files, this may take a while");
		//parse through elements in that table
		for(sometocnum = 0; sometocnum < hdr2.struct4_enum; sometocnum++)
		{
			//see if file has AUDIINDX
			fseeko64(data, struct4_ptr[sometocnum].off+0x8, SEEK_SET);
			fread(magicchk, 1, 8, data);
			if(memcmp(magicchk, cmpaudwant, 8) == 0)
			{
				//it does, read out number of files
				uint32_t audnum, skip;
				fseeko64(data, struct4_ptr[sometocnum].off+0x14, SEEK_SET);
				fread(&audnum, 1, 4, data);
				if(audnum < AUDIOMAXF)
				{
					fread(magicchk, 1, 4, data);
					//TNID label, skipping over it
					if(memcmp(magicchk, tnid, 4) == 0)
					{
						fread(&skip, 1, 4, data);
						fseeko64(data, skip, SEEK_CUR);
						fread(magicchk, 1, 4, data);
					}
					//NMOF label, names for the tracks
					if(memcmp(magicchk, nmof, 4) == 0)
					{
						uint32_t nmoffsize;
						fread(&nmoffsize, 1, 4, data);
						fread(name_offset, 1, nmoffsize, data);
						fread(magicchk, 1, 4, data);
						//ADOF label, offset and length for the tracks
						if(memcmp(magicchk, adof, 4) == 0)
						{
							uint32_t adoffsize;
							fread(&adoffsize, 1, 4, data);
							fread(audio_offset, 1, adoffsize ,data);
							fread(magicchk, 1, 4, data);
							//TNNM label, filenames, skip for now
							if(memcmp(magicchk, tnnm, 4) == 0)
							{
								fread(&skip, 1, 4, data);
								fseeko64(data, skip, SEEK_CUR);
								fread(magicchk, 1, 4, data);
								//JUNK label, skip over this too
								if(memcmp(magicchk, junk, 4) == 0)
								{
									fread(&skip, 1, 4, data);
									fseeko64(data, skip, SEEK_CUR);
									fread(magicchk, 1, 4, data);
								}
								//PACK label
								if(memcmp(magicchk, pack, 4) == 0)
								{
									uint32_t afcnt;
									for(afcnt = 0; afcnt < audnum; afcnt++)
									{
										fseeko64(data, struct4_ptr[sometocnum].off+audio_offset[afcnt].off, SEEK_SET);
										fread(magicchk, 1, 4, data);
										//OPUS label, what we actually write
										if(memcmp(magicchk, opus, 4) == 0)
										{
											fseeko64(data, struct4_ptr[sometocnum].off+name_offset[afcnt], SEEK_SET);
											fread(audio_name, 1, FILENAMELEN, data);
											audio_name[FILENAMELEN-1] = '\0';
											snprintf(filewritename, FILENAMELEN, "lopus/%s.lopus", audio_name);
											FILE *lf = fopen(filewritename, "wb");
											if(lf)
											{
												fseeko64(data, struct4_ptr[sometocnum].off+audio_offset[afcnt].off, SEEK_SET);
												writefile(data, lf, iobuf, (uint64_t)audio_offset[afcnt].len);
												fclose(lf);
											}
										}
										else
											printf("Expected OPUS, unexpected label %.4s\n", magicchk);
									}
								}
								else
									printf("Expected PACK, unexpected label %.4s\n", magicchk);
							}
							else
								printf("Expected TNNM, unexpected label %.4s\n", magicchk);
						}
						else
							printf("Expected ADOF, unexpected label %.4s\n", magicchk);
					}
					else
						printf("Expected NMOF, unexpected label %.4s\n", magicchk);
				}
				else
					printf("Too many tracks in pack (%i)\n", audnum);
			}
			else if(memcmp(magicchk, cmpaudskip, 8) == 0)
			{
				//printf("Audio TOC Skip\n");
				//some form of TOC for the bgm, skip
			}
			else
			{
				fseeko64(data, struct4_ptr[sometocnum].off+0x1F, SEEK_SET);
				fread(magicchk, 1, 4, data);
				if(memcmp(magicchk, cmpvidwant, 4) == 0)
				{
					//printf("webm\n");
					//no clue where names are, just write it with a number
					snprintf(filewritename, FILENAMELEN, "webm/%04x.webm", webmcnt++);
					FILE *wf = fopen(filewritename, "wb");
					if(wf)
					{
						fseeko64(data, struct4_ptr[sometocnum].off, SEEK_SET);
						writefile(data, wf, iobuf, struct4_ptr[sometocnum].len);
						fclose(wf);
					}
				}
				else
					printf("Unkown file type at file %i\n", sometocnum);
			}
		}
		free(raw_struct4_buf);
	}
	else
	{
		printf("Attempting to extract %s\n", argv[1]);
		if(strlen(argv[1]) > sizeof(streamname) && memcmp(argv[1], streamname, sizeof(streamname)) == 0)
		{
			puts("Stream file, looking in lower structs");
			//skip to struct 2
			fseeko64(data, hdr1.hdr2_off+sizeof(arc_hdr2), SEEK_SET);
			fseeko64(data, hdr2_struct1_len, SEEK_CUR);
			//read struct 2
			printf("Reading struct 2 (struct len %08x, total %08x bytes)\n", sizeof(hdr2_struct2), hdr2_struct2_len);
			void *raw_struct2_buf = malloc(hdr2_struct2_len);
			fread(raw_struct2_buf, 1, hdr2_struct2_len, data);
			hdr2_struct2 *struct2_ptr = (hdr2_struct2*)raw_struct2_buf;
			//read struct 3
			printf("Reading struct 3 (struct len %08x, total %08x bytes)\n", sizeof(hdr2_struct3), hdr2_struct3_len);
			void *raw_struct3_buf = malloc(hdr2_struct3_len);
			fread(raw_struct3_buf, 1, hdr2_struct3_len, data);
			hdr2_struct3 *struct3_ptr = (hdr2_struct3*)raw_struct3_buf;
			//read struct 4
			printf("Reading struct 4 (struct len %08x, total %08x bytes)\n", sizeof(hdr2_struct4), hdr2_struct4_len);
			void *raw_struct4_buf = malloc(hdr2_struct4_len);
			fread(raw_struct4_buf, 1, hdr2_struct4_len, data);
			hdr2_struct4 *struct4_ptr = (hdr2_struct4*)raw_struct4_buf;
			//hash filename
			uint32_t thecrc32 = crc32simple(argv[1], strlen(argv[1]));
			printf("Seaching for file CRC32 %08x\n", thecrc32);
			//parse struct 2
			uint32_t s2num;
			for(s2num = 0; s2num < hdr2.struct1_2_enum; s2num++)
			{
				if(struct2_ptr[s2num].fullpathhash == thecrc32 && struct2_ptr[s2num].fullpathhashlen == strlen(argv[1]))
				{
					printf("Found file at struct 2 entry %08x!\n", s2num);
					uint32_t s3_enum = struct2_ptr[s2num].struct3_enum[0] | (struct2_ptr[s2num].struct3_enum[1] << 8) 
									| (struct2_ptr[s2num].struct3_enum[2] << 16);
					if(struct2_ptr[s2num].numfiles != 0) //TODO: figure out what this exactly means
						printf("Num files seems to be %08x, only writing the first\n", struct2_ptr[s2num].numfiles);
					uint32_t s4_enum = struct3_ptr[s3_enum].struct4_enum;
					printf("Writing struct 4 entry %08x\n", s4_enum);
					uint64_t file_offset = struct4_ptr[s4_enum].off;
					uint32_t file_size = struct4_ptr[s4_enum].len;
					write_found_file(data, iobuf, argv[1], file_offset, file_size, false, 0);
					break;
				}
			}
			//done parsing
			free(raw_struct2_buf);
			free(raw_struct3_buf);
			free(raw_struct4_buf);
		}
		else
		{
			puts("Non-stream file, looking in upper structs");
			//skip to struct 6
			fseeko64(data, hdr1.hdr2_off+sizeof(arc_hdr2), SEEK_SET);
			fseeko64(data, hdr2_struct1_len, SEEK_CUR);
			fseeko64(data, hdr2_struct2_len, SEEK_CUR);
			fseeko64(data, hdr2_struct3_len, SEEK_CUR);
			fseeko64(data, hdr2_struct4_len, SEEK_CUR);
			fseeko64(data, hdr2_struct5_len, SEEK_CUR);
			//read struct 6
			printf("Reading struct 6 (struct len %08x, total %08x bytes)\n", sizeof(hdr2_struct6), hdr2_struct6_len);
			void *raw_struct6_buf = malloc(hdr2_struct6_len);
			fread(raw_struct6_buf, 1, hdr2_struct6_len, data);
			hdr2_struct6 *struct6_ptr = (hdr2_struct6*)raw_struct6_buf;
			printf("First struct 6 crc32 %08x\n", struct6_ptr[0].fullgrouphash);
			//read struct 7
			printf("Reading struct 7 (struct len %08x, total %08x bytes)\n", sizeof(hdr2_struct7), hdr2_struct7_len);
			void *raw_struct7_buf = malloc(hdr2_struct7_len);
			fread(raw_struct7_buf, 1, hdr2_struct7_len, data);
			hdr2_struct7 *struct7_ptr = (hdr2_struct7*)raw_struct7_buf;
			//skip struct 8
			printf("Skipping over struct 8 (struct len %08x, total %08x bytes)\n", sizeof(hdr2_struct8), hdr2_struct8_len);
			fseeko64(data, hdr2_struct8_len, SEEK_CUR);
			//read struct 9
			printf("Reading struct 9 (struct len %08x, total %08x bytes)\n", sizeof(hdr2_struct9), hdr2_struct9_len);
			void *raw_struct9_buf = malloc(hdr2_struct9_len);
			fread(raw_struct9_buf, 1, hdr2_struct9_len, data);
			hdr2_struct9 *struct9_ptr = (hdr2_struct9*)raw_struct9_buf;
			printf("First struct 9 crc32 %08x\n", struct9_ptr[0].fullpathhash);
			//read struct 10
			printf("Reading struct 10 (struct len %08x, total %08x bytes)\n", sizeof(hdr2_struct10), hdr2_struct10_len);
			void *raw_struct10_buf = malloc(hdr2_struct10_len);
			fread(raw_struct10_buf, 1, hdr2_struct10_len, data);
			hdr2_struct10 *struct10_ptr = (hdr2_struct10*)raw_struct10_buf;
			//hash filename
			uint32_t thecrc32 = crc32simple(argv[1], strlen(argv[1]));
			printf("Seaching for file CRC32 %08x\n", thecrc32);
			//parse struct 9
			uint32_t s9num;
			for(s9num = 0; s9num < hdr2.struct9_14_enum1; s9num++)
			{
				if(struct9_ptr[s9num].fullpathhash == thecrc32 && struct9_ptr[s9num].fullpathhashlen == strlen(argv[1]))
				{
					printf("Found file at struct 9 entry %08x!\n", s9num);
					uint32_t s6_11_enum = struct9_ptr[s9num].struct6_11_enum[0] | (struct9_ptr[s9num].struct6_11_enum[1] << 8) 
									| (struct9_ptr[s9num].struct6_11_enum[2] << 16);
					uint32_t s7_enum = struct6_ptr[s6_11_enum].struct7_enum[0] | (struct6_ptr[s6_11_enum].struct7_enum[1] << 8) 
									| (struct6_ptr[s6_11_enum].struct7_enum[2] << 16);
					uint32_t s10_enum = struct9_ptr[s9num].struct10_enum;
					uint64_t file_offset = hdr1.fil1_off+struct7_ptr[s7_enum].base_offset+(struct10_ptr[s10_enum].file_local_offset<<2);
					uint32_t file_size = struct10_ptr[s10_enum].file_len_cmp;
					if(struct10_ptr[s10_enum].flags == 0x03000000 && struct10_ptr[s10_enum].file_len_decmp)
					{
						puts("File compressed, attempting to decompress");
						write_found_file(data, iobuf, argv[1], file_offset, file_size, true, struct10_ptr[s10_enum].file_len_decmp);
					}
					else
					{
						if(struct10_ptr[s10_enum].flags != 0)
							printf("Unknown struct 10 flag %08x\n", struct10_ptr[s10_enum].flags);
						write_found_file(data, iobuf, argv[1], file_offset, file_size, false, 0);
					}
					break;
				}
			}
			//done parsing
			free(raw_struct6_buf);
			free(raw_struct7_buf);
			free(raw_struct9_buf);
			free(raw_struct10_buf);
		}
	}
	puts("Done!");
	fclose(data);
	free(iobuf);
	return 0;
}
