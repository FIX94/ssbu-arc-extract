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

// 16MB i/o buf
#define IOBUFLEN 16*1024*1024

// main file header
typedef struct _arc_hdr1 {
	uint64_t magic; // 10 32 54 76 98 EF CD AB
	uint64_t hdrsize; // 0x38
	int64_t fil1_off; // first offset shift
	int64_t fil2_off;
	int64_t hdr2_off; // first big header
	int64_t hdr3_off; // second big header
	uint64_t empty;
} arc_hdr1;

//at hdr2_off
typedef struct _arc_hdr2 {
	uint32_t hdrlen;
	// handles file structure after 2gb, can be compressed
	uint32_t struct6_11_enum; //0x64EE
	uint32_t struct7_enum_part1; //0x8DE9, first half of 0x8F83
	uint32_t struct9_14_enum1; //0x73BBA
	uint32_t struct10_enum_part1; //0x860D1, first half of 0x89B11
	uint32_t struct13_enum; //0x73376
	uint32_t struct8_enum; //0x64D1
	uint32_t struct9_14_enum2; //0x73BBA
	uint32_t struct7_enum_part2; //0x19A, second half of 0x8DE9
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
typedef struct _hdr2_struct1 {
	uint32_t fullpathhash;
	uint8_t fullpathhashlen;
	uint8_t struct2_index[3];
} hdr2_struct1;

// at hdr2_off+0x4C68, 0x7200 bytes, 0x980 entries, second hash table sorts by increasing index
typedef struct _hdr2_struct2 {
	uint32_t fullpathhash;
	uint8_t fullpathhashlen;
	//this index increases by more than 1 depending
	//on what the numfiles value is below
	uint8_t struct3_index[3];
	//00=1 entry for struct 3
	//01=0xE entries for struct 3
	//02=0x5 entries for struct 3
	uint32_t numfiles;
} hdr2_struct2;

// at hdr2_off+0xBE68, 0x5404 bytes, 0x1501 entries, file id table, to point to exact file number below
typedef struct _hdr2_struct3 {
	uint32_t struct4_index;
} hdr2_struct3;

// at hdr2_off+0x1126C, 0xAA10 bytes, file offset table indexed with table above
typedef struct _hdr2_struct4 {
	uint64_t len;
	uint64_t off;
} hdr2_struct4;

// TODO: figure out all struct uses below
// at hdr2_off+0x1BC7C, 0xA8 bytes, 0xE elements
typedef struct _hdr2_struct5 {
	uint32_t unk1;
	uint32_t unk2;
	uint32_t unk3;
} hdr2_struct5;

// at hdr2_off+0x1BD24, 0x148058 bytes, 0x64EE elements
typedef struct _hdr2_struct6 {
	uint32_t somehash1;
	uint8_t somehashlen1;
	uint8_t struct7_enum[3]; //needed to get full file offset
	uint32_t somehash2;
	uint8_t somehashlen2;
	uint8_t unk1[3];
	uint32_t somehash3;
	uint8_t somehashlen3;
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
typedef struct _hdr2_struct7 {
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
typedef struct _hdr2_struct8 {
	uint32_t fullpathhash;
	uint8_t fullpathhashlen;
	uint8_t unk[3];
} hdr2_struct8;

// at hdr2_off+0x291658, 0x1215510 bytes, 0x73BBA entries, name multi hashes
typedef struct _hdr2_struct9 {
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
typedef struct _hdr2_struct10 {
	// multiple local offset by 4 and add on top of struct 7 offset to get full file offset
	uint32_t file_local_offset;
	uint32_t file_len_cmp;
	uint32_t file_len_decmp;
	uint32_t unk2; //possibly compressed flag, compression is done with zstd
} hdr2_struct10;

// at hdr2_off+0x1D41C78, 0x32770 bytes, 0x64EE entries
typedef struct _hdr2_struct11 {
	uint32_t somehash;
	uint8_t somehashlen;
	//every entry just adds 1 to this
	uint8_t num[3];
} hdr2_struct11;

// at hdr2_off+0x1D743E8, 8 bytes
typedef struct _hdr2_struct12_hdr {
	uint32_t totalnums; //0x73376
	uint32_t struct11_elements; //0x400
} hdr2_struct12_hdr;

// at hdr2_off+0x1D743F0, 0x2000 bytes, 0x400 entries
typedef struct _hdr2_struct12 {
	uint32_t somenum; //counts from 0 up to 0x73376
	uint32_t someadd; //value added each entry
} hdr2_struct12;

// at hdr2_off+0x1D763F0, 0x399BB0 bytes, 0x73376 entries
typedef struct _hdr2_struct13 {
	uint32_t fullpathhash;
	uint8_t fullpathhashlen;
	uint8_t someindex[3]; //possibly similar index to struct 6
} hdr2_struct13;

// at hdr2_off+0x210FFA0, 0x39DDD0 bytes, 0x73BBA entries
typedef struct _hdr2_struct14 {
	uint8_t empty[5];
	//every entry just adds 1 to this
	uint8_t num[3];
} hdr2_struct14;

// done, all 0x24ADD70 bytes of hdr2 processed,
// hdr3 follows right behind it, TODO: look at hdr3 in more detail
typedef struct _arc_hdr3 {
	uint32_t hdrlen;
	uint32_t empty;
	// also used for files past 2gb in some way I think
	uint32_t struct1_2_enum; //0x7541
	uint32_t struct3_4_5_enum1; //0x80272
	uint32_t struct3_4_5_enum2; //0x80272
} arc_hdr3;

//at hdr3_off+0x14, 0x3AA08 bytes, 0x7541 entries
typedef struct _hdr3_struct1 {
	uint32_t somehash;
	uint8_t somehashlen;
	uint8_t unk1[3];
} hdr3_struct1;

//at hdr3_off+0x3AA1C, 0xEA820 bytes, 0x7541 entries
typedef struct _hdr3_struct2 {
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
typedef struct _hdr3_struct3 {
	uint32_t somehash;
	uint8_t somehashlen;
	uint8_t unk1[3];
} hdr3_struct3;

//at hdr_off+0x5265CC, 0x1E3F54 bytes, 0x80272 entries
typedef struct _hdr3_struct4 {
	uint32_t num; //just counts up
} hdr3_struct4;

// at hdr3_off+0x726F94, 0xF1FAA0 bytes, 0x80272 entries, name multi hashes
typedef struct _hdr3_struct5 {
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
typedef struct _nustoc {
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
static void writefile(FILE *in, FILE *out, uint8_t *buf, uint64_t len)
{
	while(len)
	{
		size_t proc = (len > IOBUFLEN) ? IOBUFLEN : (size_t)len;
		fread(buf, 1, proc, in);
		fwrite(buf, 1, proc, out);
		len -= proc;
	}
}

int main()
{
	puts("Smash Ultimate LOpus/WebM Extract WIP-1 by FIX94");
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
	//make sure output folders exist
	mkdir("lopus");
	mkdir("webm");
	void *iobuf = malloc(IOBUFLEN);
	//skip all the way to struct 4, the offset table
	fseeko64(data, hdr1.hdr2_off+sizeof(arc_hdr2), SEEK_SET);
	fseeko64(data, sizeof(hdr2_struct1)*hdr2.struct1_2_enum, SEEK_CUR);
	fseeko64(data, sizeof(hdr2_struct2)*hdr2.struct1_2_enum, SEEK_CUR);
	fseeko64(data, sizeof(hdr2_struct3)*hdr2.struct3_enum, SEEK_CUR);
	//read struct 4
	void *rawtoc = malloc(sizeof(hdr2_struct4)*hdr2.struct4_enum);
	fread(rawtoc, 1, sizeof(hdr2_struct4)*hdr2.struct4_enum, data);
	hdr2_struct4 *sometoc = (hdr2_struct4*)rawtoc;
	uint16_t sometocnum;
	uint16_t webmcnt = 0;
	uint8_t magicchk[8];
	puts("Extracting, this may take a while");
	//parse through elements in that table
	for(sometocnum = 0; sometocnum < hdr2.struct4_enum; sometocnum++)
	{
		//see if file has AUDIINDX
		fseeko64(data, sometoc[sometocnum].off+0x8, SEEK_SET);
		fread(magicchk, 1, 8, data);
		if(memcmp(magicchk, cmpaudwant, 8) == 0)
		{
			//it does, read out number of files
			uint32_t audnum, skip;
			fseeko64(data, sometoc[sometocnum].off+0x14, SEEK_SET);
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
									fseeko64(data, sometoc[sometocnum].off+audio_offset[afcnt].off, SEEK_SET);
									fread(magicchk, 1, 4, data);
									//OPUS label, what we actually write
									if(memcmp(magicchk, opus, 4) == 0)
									{
										fseeko64(data, sometoc[sometocnum].off+name_offset[afcnt], SEEK_SET);
										fread(audio_name, 1, FILENAMELEN, data);
										audio_name[FILENAMELEN-1] = '\0';
										snprintf(filewritename, FILENAMELEN, "lopus/%s.lopus", audio_name);
										FILE *lf = fopen(filewritename, "wb");
										if(lf)
										{
											fseeko64(data, sometoc[sometocnum].off+audio_offset[afcnt].off, SEEK_SET);
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
			fseeko64(data, sometoc[sometocnum].off+0x1F, SEEK_SET);
			fread(magicchk, 1, 4, data);
			if(memcmp(magicchk, cmpvidwant, 4) == 0)
			{
				//printf("webm\n");
				//no clue where names are, just write it with a number
				snprintf(filewritename, FILENAMELEN, "webm/%04x.webm", webmcnt++);
				FILE *wf = fopen(filewritename, "wb");
				if(wf)
				{
					fseeko64(data, sometoc[sometocnum].off, SEEK_SET);
					writefile(data, wf, iobuf, sometoc[sometocnum].len);
					fclose(wf);
				}
			}
			else
				printf("Unkown file type at file %i\n", sometocnum);
		}
	}
	puts("Done!");
	fclose(data);
	free(rawtoc);
	free(iobuf);
	return 0;
}
