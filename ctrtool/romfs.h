#ifndef __ROMFS_H__
#define __ROMFS_H__

#include "types.h"
#include "info.h"
#include "ctr.h"
#include "filepath.h"
#include "settings.h"
#include "ivfc.h"

#define ROMFS_MAXNAMESIZE	254		// limit set by ctrtool

typedef struct
{
	u8 magic[4];
} romfs_header;

typedef struct
{
	u8 offset[4];
	u8 size[4];
} romfs_sectionheader;

typedef struct
{
	u8 headersize[4];
	romfs_sectionheader section[4];
	u8 dataoffset[4];
} romfs_infoheader;


typedef struct
{
	u8 parentoffset[4];
	u8 siblingoffset[4];
	u8 childoffset[4];
	u8 fileoffset[4];
	u8 weirdoffset[4]; // this one is weird. it always points to a dir entry, but seems unrelated to the romfs structure.
	u8 namesize[4];
	u8 name[ROMFS_MAXNAMESIZE];
} romfs_direntry;

typedef struct
{
	u8 parentdiroffset[4];
	u8 siblingoffset[4];
	u8 dataoffset[8];
	u8 datasize[8];
	u8 weirdoffset[4]; // this one is also weird. it always points to a file entry, but seems unrelated to the romfs structure.
	u8 namesize[4];
	u8 name[ROMFS_MAXNAMESIZE];
} romfs_fileentry;


typedef struct
{
	FILE* file;
	settings* usersettings;
	u32 offset;
	u32 size;
	romfs_header header;
	romfs_infoheader infoheader;
	u8* dirblock;
	u32 dirblocksize;
	u8* fileblock;
	u32 fileblocksize;
	u32 datablockoffset;
	u32 infoblockoffset;
	romfs_direntry direntry;
	romfs_fileentry fileentry;
	ivfc_context ivfc;
} romfs_context;

void romfs_init(romfs_context* ctx);
void romfs_set_file(romfs_context* ctx, FILE* file);
void romfs_set_offset(romfs_context* ctx, u32 offset);
void romfs_set_size(romfs_context* ctx, u32 size);
void romfs_set_usersettings(romfs_context* ctx, settings* usersettings);
void romfs_test(romfs_context* ctx);
int  romfs_dirblock_read(romfs_context* ctx, u32 diroffset, u32 dirsize, void* buffer);
int  romfs_dirblock_readentry(romfs_context* ctx, u32 diroffset, romfs_direntry* entry);
int  romfs_fileblock_read(romfs_context* ctx, u32 fileoffset, u32 filesize, void* buffer);
int  romfs_fileblock_readentry(romfs_context* ctx, u32 fileoffset, romfs_fileentry* entry);
void romfs_visit_dir(romfs_context* ctx, u32 diroffset, u32 depth, u32 actions, filepath* rootpath);
u32 romfs_visit_file(romfs_context *ctx, u32 fileoffset, u32 depth, u32 actions, filepath *rootpath);
void romfs_extract_datafile(romfs_context* ctx, u64 offset, u64 size, filepath* path);
void romfs_process(romfs_context* ctx, u32 actions);
void romfs_print(romfs_context* ctx);

#endif // __ROMFS_H__
