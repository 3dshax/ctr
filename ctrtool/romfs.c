#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "types.h"
#include "romfs.h"
#include "utils.h"
#include "filepath.h"

void romfs_init(romfs_context* ctx)
{
	memset(ctx, 0, sizeof(romfs_context));
	ivfc_init(&ctx->ivfc);
}

void romfs_set_file(romfs_context* ctx, FILE* file)
{
	ctx->file = file;
}

void romfs_set_offset(romfs_context* ctx, u32 offset)
{
	ctx->offset = offset;
}

void romfs_set_size(romfs_context* ctx, u32 size)
{
	ctx->size = size;
}

void romfs_set_usersettings(romfs_context* ctx, settings* usersettings)
{
	ctx->usersettings = usersettings;
}



void romfs_process(romfs_context* ctx, u32 actions)
{
	u32 dirblockoffset = 0;
	u32 dirblocksize = 0;
	u32 fileblockoffset = 0;
	u32 fileblocksize = 0;


	ivfc_set_offset(&ctx->ivfc, ctx->offset);
	ivfc_set_size(&ctx->ivfc, ctx->size);
	ivfc_set_file(&ctx->ivfc, ctx->file);
	ivfc_set_usersettings(&ctx->ivfc, ctx->usersettings);
	ivfc_process(&ctx->ivfc, actions);

	fseek(ctx->file, ctx->offset, SEEK_SET);
	fread(&ctx->header, 1, sizeof(romfs_header), ctx->file);

	if (getle32(ctx->header.magic) != MAGIC_IVFC)
	{
		fprintf(stdout, "Error, RomFS corrupted\n");
		return;
	}

	ctx->infoblockoffset = ctx->offset + 0x1000;

	fseek(ctx->file, ctx->infoblockoffset, SEEK_SET);
	fread(&ctx->infoheader, 1, sizeof(romfs_infoheader), ctx->file);
	
	if (getle32(ctx->infoheader.headersize) != sizeof(romfs_infoheader))
	{
		fprintf(stderr, "Error, info header mismatch\n");
		return;
	}

	dirblockoffset = ctx->infoblockoffset + getle32(ctx->infoheader.section[1].offset);
	dirblocksize = getle32(ctx->infoheader.section[1].size);
	fileblockoffset = ctx->infoblockoffset + getle32(ctx->infoheader.section[3].offset);
	fileblocksize = getle32(ctx->infoheader.section[3].size);

	ctx->dirblock = malloc(dirblocksize);
	ctx->dirblocksize = dirblocksize;
	ctx->fileblock = malloc(fileblocksize);
	ctx->fileblocksize = fileblocksize;

	ctx->datablockoffset = ctx->infoblockoffset + getle32(ctx->infoheader.dataoffset);

	if (ctx->dirblock)
	{
		fseek(ctx->file, dirblockoffset, SEEK_SET);
		fread(ctx->dirblock, 1, dirblocksize, ctx->file);
	}
	else perror("ctx->dirblock\n");

	if (ctx->fileblock)
	{
		fseek(ctx->file, fileblockoffset, SEEK_SET);
		fread(ctx->fileblock, 1, fileblocksize, ctx->file);
	}
	else perror("ctx->fileblock\n");

	if (actions & InfoFlag)
		romfs_print(ctx);

	romfs_visit_dir(ctx, 0, 0, actions, settings_get_romfs_dir_path(ctx->usersettings));

}

int romfs_dirblock_read(romfs_context* ctx, u32 diroffset, u32 dirsize, void* buffer)
{
	if (!ctx->dirblock)
		return 0;

	if (diroffset+dirsize > ctx->dirblocksize)
		return 0;

	memcpy(buffer, ctx->dirblock + diroffset, dirsize);
	return 1;
}

int romfs_dirblock_readentry(romfs_context* ctx, u32 diroffset, romfs_direntry* entry)
{
	u32 size_without_name = sizeof(romfs_direntry) - ROMFS_MAXNAMESIZE;
	u32 namesize;


	if (!ctx->dirblock)
		return 0;

	if (!romfs_dirblock_read(ctx, diroffset, size_without_name, entry))
		return 0;
	
	namesize = getle32(entry->namesize);
	if (namesize > (ROMFS_MAXNAMESIZE-2))
		namesize = (ROMFS_MAXNAMESIZE-2);
	memset(entry->name + namesize, 0, 2);
	if (!romfs_dirblock_read(ctx, diroffset + size_without_name, namesize, entry->name))
		return 0;

	return 1;
}


int romfs_fileblock_read(romfs_context* ctx, u32 fileoffset, u32 filesize, void* buffer)
{
	if (!ctx->fileblock)
		return 0;

	if (fileoffset+filesize > ctx->fileblocksize)
		return 0;

	memcpy(buffer, ctx->fileblock + fileoffset, filesize);
	return 1;
}

int romfs_fileblock_readentry(romfs_context* ctx, u32 fileoffset, romfs_fileentry* entry)
{
	u32 size_without_name = sizeof(romfs_fileentry) - ROMFS_MAXNAMESIZE;
	u32 namesize;


	if (!ctx->fileblock)
		return 0;

	if (!romfs_fileblock_read(ctx, fileoffset, size_without_name, entry))
		return 0;
	
	namesize = getle32(entry->namesize);
	if (namesize > (ROMFS_MAXNAMESIZE-2))
		namesize = (ROMFS_MAXNAMESIZE-2);
	memset(entry->name + namesize, 0, 2);
	if (!romfs_fileblock_read(ctx, fileoffset + size_without_name, namesize, entry->name))
		return 0;

	return 1;
}



void romfs_visit_dir(romfs_context* ctx, u32 diroffset, u32 depth, u32 actions, filepath* rootpath)
{
	u32 siblingoffset;
	u32 childoffset;
	u32 fileoffset;
	filepath currentpath;
	romfs_direntry* entry = &ctx->direntry;


	if (!romfs_dirblock_readentry(ctx, diroffset, entry))
		return;


//	fprintf(stdout, "%08X %08X %08X %08X %08X ", 
//			getle32(entry->parentoffset), getle32(entry->siblingoffset), getle32(entry->childoffset), 
//			getle32(entry->fileoffset), getle32(entry->weirdoffset));
//	fwprintf(stdout, L"%ls\n", entry->name);


	if (rootpath && rootpath->valid)
	{
		filepath_copy(&currentpath, rootpath);
		filepath_append_utf16(&currentpath, entry->name);
		if (currentpath.valid)
		{
			makedir(currentpath.pathname);
		}
		else
		{
			fprintf(stderr, "Error creating directory in root %s\n", rootpath->pathname);
			return;
		}
	}
	else
	{
		filepath_init(&currentpath);

		if (settings_get_list_romfs_files(ctx->usersettings))
		{
			u32 i;

			for(i=0; i<depth; i++)
				printf(" ");
			fwprintf(stdout, L"%ls\n", entry->name);
		}
	}
	

	siblingoffset = getle32(entry->siblingoffset);
	childoffset = getle32(entry->childoffset);
	fileoffset = getle32(entry->fileoffset);

	while(fileoffset != (~0))
	{
		fileoffset=romfs_visit_file(ctx, fileoffset, depth+1, actions, &currentpath);
	}

	if (childoffset != (~0))
		romfs_visit_dir(ctx, childoffset, depth+1, actions, &currentpath);

	if (siblingoffset != (~0))
		romfs_visit_dir(ctx, siblingoffset, depth, actions, rootpath);
}


u32 romfs_visit_file(romfs_context *ctx, u32 fileoffset, u32 depth, u32 actions, filepath *rootpath)
{
	u32 siblingoffset = 0;
	filepath currentpath;
	romfs_fileentry* entry = &ctx->fileentry;


	if (!romfs_fileblock_readentry(ctx, fileoffset, entry))
		return 0;


//	fprintf(stdout, "%08X %08X %016llX %016llX %08X ", 
//		getle32(entry->parentdiroffset), getle32(entry->siblingoffset), ctx->datablockoffset+getle64(entry->dataoffset),
//			getle64(entry->datasize), getle32(entry->unknown));
//	fwprintf(stdout, L"%ls\n", entry->name);

	if (rootpath && rootpath->valid)
	{
		filepath_copy(&currentpath, rootpath);
		filepath_append_utf16(&currentpath, entry->name);
		if (currentpath.valid)
		{
			fprintf(stdout, "Saving %s...\n", currentpath.pathname);
			romfs_extract_datafile(ctx, getle64(entry->dataoffset), getle64(entry->datasize), &currentpath);
		}
		else
		{
			fprintf(stderr, "Error creating directory in root %s\n", rootpath->pathname);
			return 0;
		}
	}
	else
	{
		filepath_init(&currentpath);
		if (settings_get_list_romfs_files(ctx->usersettings))
		{
			u32 i;

			for(i=0; i<depth; i++)
				printf(" ");
			fwprintf(stdout, L"%ls\n", entry->name);
		}
	}

	siblingoffset = getle32(entry->siblingoffset);
	return siblingoffset;
}

void romfs_extract_datafile(romfs_context* ctx, u64 offset, u64 size, filepath* path)
{
	FILE* outfile = 0;
	u32 max;
	u8 buffer[4096];


	if (path == 0 || path->valid == 0)
		goto clean;

	offset += ctx->datablockoffset;
	if ( (offset >> 32) )
	{
		fprintf(stderr, "Error, support for 64-bit offset not yet implemented.\n");
		goto clean;
	}

	fseek(ctx->file, offset, SEEK_SET);
	outfile = fopen(path->pathname, "wb");
	if (outfile == 0)
	{
		fprintf(stderr, "Error opening file for writing\n");
		goto clean;
	}

	while(size)
	{
		max = sizeof(buffer);
		if (max > size)
			max = size;

		if (max != fread(buffer, 1, max, ctx->file))
		{
			fprintf(stderr, "Error reading file\n");
			goto clean;
		}

		if (max != fwrite(buffer, 1, max, outfile))
		{
			fprintf(stderr, "Error writing file\n");
			goto clean;
		}

		size -= max;
	}
clean:
	if (outfile)
		fclose(outfile);
}


void romfs_print(romfs_context* ctx)
{
	u32 i;

	fprintf(stdout, "\nRomFS:\n");

	fprintf(stdout, "Header size:            0x%08X\n", getle32(ctx->infoheader.headersize));
	for(i=0; i<4; i++)
	{
		fprintf(stdout, "Section %d offset:       0x%08X\n", i, ctx->offset + 0x1000 + getle32(ctx->infoheader.section[i].offset));
		fprintf(stdout, "Section %d size:         0x%08X\n", i, getle32(ctx->infoheader.section[i].size));
	}

	fprintf(stdout, "Data offset:            0x%08X\n", ctx->offset + 0x1000 + getle32(ctx->infoheader.dataoffset));
}
