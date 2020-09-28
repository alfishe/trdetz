#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include "defines.h"
#include "trd.h"

#define ARGS_MAX 10

#ifdef __GNUC__
#pragma pack(push,1)
#else
#pragma pack(push)
#endif
#pragma pack(1)

struct hobeta_header {
	struct trd_fname filename;	
	BYTE start_l;
	BYTE start_h;
	BYTE length_l;
	BYTE length_h;	
	BYTE sec_bytes_l;
	BYTE sec_bytes_h; /*i.e. num of sectors*/
	BYTE hdr_crc_l;
	BYTE hdr_crc_h;
};

#pragma pack(pop)

static void hobeta_update_checksum(struct hobeta_header *hob)
{
	int i, checksum=0;
	
	for (i=0; i<=14; checksum = checksum + (((BYTE *)hob)[i]) + i, i++);	
	hob->hdr_crc_l = (checksum & 0xFF);
	hob->hdr_crc_h = (checksum >> 8);
	
	return;
}

static void usage()
{
	printf("trdetz the .trd file rummager\nver %s by boo_boo [boo_boo(^at^)inbox.ru]\n",VERSION);
	printf("this is free and open source software, redistributable under zlib/libpng license.\n");
	printf("\n");
	printf("USAGE: trdetz [flags] trd_file command [arguments]\n");
	printf("Numeric args may be given as [dec], 0x[hex], or 0[oct]\n");
	printf("\n");
	printf("Flags:\n");
	printf("-df[=del_file_marker] : handle deleted files (and optionally set deleted file marker, which is '?' by default)\n");
	printf("-fs : use num_sectors*256 as file length\n");
	printf("-hob : use hobeta format when exporting or importing TR-DOS file (affects 'fread' and 'fwrite' commands)\n");	
	printf("\n");
	printf("Commands:\n");
	printf("dinfo : print disc summary\n");
	printf("cat [fname] : print catalogue (or single file) info\n");
	printf("secread <bin_fname> <track> <sector> <num_sectors> : copy sectors from trd to binary file\n");
	printf("secwrite <bin_fname> <track> <sector> <num_sectors> : copy sectors from binary file to trd\n");
	printf("fread <fname> [bin_fname] : copy file from trd to binary\n");
	printf("fwrite <bin_fname> [fname] : copy binary file to trd\n");
	printf("del <fname> : delete file from trd catalogue\n");
	printf("ren <old_fname> <new_fname> : rename file in trd catalogue (use with -df to undelete file)\n");
	printf("move : pack disc space (as in TR-DOS 'move' command)\n");
	printf("fprop <fname> <property name> <property value> : change file property\n");
	printf("       where property name is one of:\n");
	printf("         start -- for file start adress\n");
	printf("         flen -- for file length in bytes\n");
	printf("         nsec -- for file length in sectors\n");
	printf("\n");
	
	return;
}

static char *cmd_args[ARGS_MAX];
static int cmd_args_num=0;

static void cmd_getargs(int argc, char *argv[], int from)
{
	int i,n;
	
	cmd_args_num=0;
	for(n=0; n < ARGS_MAX; ++n) cmd_args[n] = NULL;
	
	for(i=from, n=0; i < argc; ++i, ++n)
	{
		cmd_args[n]=argv[i];
		cmd_args_num++;
	}
	
	return;
}

int main(int argc, char *argv[])
{
	char *command = NULL;
	char *trd_fname = NULL;
	char *endptr;
	char *fname;
	int clarg, i, prn, sector, track, num_sectors, num_bytes, ivalue, num_files, fnum;
	char strbuff[100];
	char with_del_files = 0;
	struct trd_cat_entry *pcat = NULL;
	char del_file_mark='?';
	char fname_ext_separator='.';
	unsigned use_cat_bytelen=1;
	unsigned hobeta_mode = 0;
	struct trd_fname trd_filename, trd_filename2;
	BYTE *bfile_buff = NULL;
	FILE *bfp = NULL;
	int is_last_file;
	struct trd_context *trd = NULL;
	struct trd_disc_info *dinfo = NULL;
	struct hobeta_header *hob = NULL;
	struct hobeta_header hob_w;
	
	if(argc < 3) goto usage;

	/*parse flags*/
	for(clarg=1; clarg < argc; ++clarg)
	{
		if(*(argv[clarg]) != '-') break;
		
		if(!strncmp(argv[clarg],"-df=",4) && strlen(argv[clarg])==5)
		{
			with_del_files=(argv[clarg])[4]; /*handle deleted files (as "[sym]rest_of_name")*/
		}
		else if(!strcmp(argv[clarg],"-df")) with_del_files='?'; /*handle deleted files (as "?rest_of_name")*/
		else if(!strcmp(argv[clarg],"-fs")) use_cat_bytelen=0;	/*use file length as given in the catalogue instead of num_sectors*256*/
		else if(!strcmp(argv[clarg],"-hob")) hobeta_mode=1; /*hobeta mode for file import/export*/
		else 
		{
			fprintf(stderr, "Unknown flag: %s\n", argv[clarg]);
			goto fail;
		}
	}
	
	if(clarg < argc) trd_fname=argv[clarg++];
	if(clarg < argc) command=argv[clarg++];
	else goto usage;
	
	if(NULL == (trd = trd_img_create()))
	{
		fprintf(stderr, "Cannot allocate TRD context\n");
		goto fail;
	}
	
	if(!trd_img_read(trd, trd_fname)) goto trd_fail;
	
	cmd_getargs(argc, argv, clarg);
	
	if(!strcmp(command,"dinfo")) /*dinfo : print disc summary*/
	{
		dinfo=trd_get_disc_info_ref(trd);
		memcpy(strbuff,&(dinfo->disc_name),8);
		strbuff[8]='\0';
		
		printf("title: %s\n",strbuff);
		printf("free_sectors: %d\n",TO_WORD(dinfo->free_sectors_h,dinfo->free_sectors_l));
		printf("regular_files: %d\n",dinfo->files - dinfo->deleted_files);
		printf("deleted_files: %d\n",dinfo->deleted_files);
		printf("next_free_track: %d\n",dinfo->next_free_track);
		printf("next_free_sector: %d\n",dinfo->next_free_sector);
		
	}
	else if(!strcmp(command,"cat")) /*cat [fname] : print catalogue (or single file) info*/
	{
		if(cmd_args[0] != NULL && !trd_parse_fname(cmd_args[0], fname_ext_separator, with_del_files, &trd_filename))
		{
			fprintf(stderr, "Invalid TRDOS filename given: %s\n",cmd_args[0]);
			goto fail;
		}
	
		prn=0;
		num_files=trd_read_byte(trd,(0x100*8)+0xE4);
		for(i=0; i< num_files; ++i)
		{
			pcat=trd_get_cat_entry_ref(trd,i);
			assert(pcat!=NULL);
			
			if(!((pcat->filename.name)[0]) || ((pcat->filename.name)[0]) == 0x01)
			{
				if(!with_del_files) continue;
				
				strncpy(strbuff+1, (char *)(pcat->filename.name + 1), 7);
				*strbuff = del_file_mark;
			}
			else strncpy(strbuff, (char *)pcat->filename.name, 8);
			strbuff[8]='\0';	
			
			if(cmd_args[0] != NULL)
			{
				if(!trd_fname_equal(&trd_filename, &(pcat->filename))) continue;
			}
				
			printf("%s.%c %3u %05d %05d %3u %3u\n",
				strbuff, 
				(char)(pcat->filename.ext), 
				pcat->sectors, 
				TO_WORD(pcat->start_h, pcat->start_l), 
				TO_WORD(pcat->length_h, pcat->length_l),
				pcat->start_track,
				pcat->start_sector
				);
			prn=1;
		}	


		if(cmd_args[0] != NULL && !prn)
		{
			fprintf(stderr, "File '%s' not found\n", cmd_args[0]);
			goto fail;			
		}
	}
	else if(!strcmp(command, "secwrite")) /*secwrite <bin_fname> <track> <sector> <num_sectors> : copy sectors from binary file to trd*/
	{
		if(cmd_args_num < 4) goto cmd_args_fail;
		
		track = strtol(cmd_args[1],&endptr,0);
		if(*endptr != '\0' || track < 0) goto cmd_args_fail;

		sector = strtol(cmd_args[2],&endptr,0);
		if(*endptr != '\0' || sector < 0) goto cmd_args_fail;
			
		num_sectors = strtol(cmd_args[3],&endptr,0);
		if(*endptr != '\0' || num_sectors < 1) goto cmd_args_fail;
		
		/*if(num_sectors > 0x100)
		{
			fprintf(stderr, "Too many sectors\n");
			goto fail;
		}*/			
		
		if(NULL == (bfile_buff = (BYTE *) malloc(num_sectors * 0x100)))
		{
			fprintf(stderr, "Failed to allocate memory\n");
			goto fail;			
		}
		memset(bfile_buff,0,num_sectors * 0x100);		

		if(NULL == (bfp = fopen(cmd_args[0],"rb")))
		{
			fprintf(stderr, "Cannot open file %s for read\n",cmd_args[0]);
			goto fail;			
		}

		if(!fread(bfile_buff,1,num_sectors * 0x100,bfp))
		{
			fprintf(stderr, "Cannot read from file %s\n",cmd_args[0]);
			goto fail;			
		}			
		
		fclose(bfp);
		
		if(!trd_write_sectors(trd, track, sector, num_sectors, bfile_buff)) goto trd_fail;
		if(!trd_img_write(trd, trd_fname)) goto trd_fail;
		free(bfile_buff);
	}
	else if(!strcmp(command, "secread")) /*secread <bin_fname> <track> <sector> <num_sectors> : copy sectors from trd to binary file*/
	{
		if(cmd_args_num < 4) goto cmd_args_fail;

		track = strtol(cmd_args[1],&endptr,0);
		if(*endptr != '\0' || track < 0) goto cmd_args_fail;

		sector = strtol(cmd_args[2],&endptr,0);
		if(*endptr != '\0' || sector < 0) goto cmd_args_fail;
			
		num_sectors = strtol(cmd_args[3],&endptr,0);
		if(*endptr != '\0' || num_sectors < 1) goto cmd_args_fail;		
		
		if(NULL == (bfile_buff = (BYTE *) malloc(num_sectors * 0x100)))
		{
			fprintf(stderr, "Failed to allocate memory\n");
			goto fail;			
		}
		memset(bfile_buff,0,num_sectors * 0x100);
		
		if(!trd_read_sectors(trd, track, sector, num_sectors, bfile_buff)) goto trd_fail;
		
		if(NULL == (bfp = fopen(cmd_args[0],"wb")))
		{
			fprintf(stderr, "Cannot open file %s for writing\n",cmd_args[0]);
			goto fail;			
		}

		if(fwrite(bfile_buff,1,num_sectors * 0x100,bfp) != num_sectors * 0x100)
		{
			fprintf(stderr, "Error writing to file %s\n",cmd_args[0]);
			goto fail;			
		}		

		free(bfile_buff);
		fclose(bfp);
	}	
	else if(!strcmp(command, "fwrite")) /*fwrite <bin_fname> [fname]: copy binary file to trd*/
	{
		if(cmd_args_num < 1) goto cmd_args_fail;
		
		if(NULL == (bfp = fopen(cmd_args[0],"rb")))
		{
			fprintf(stderr, "Cannot open file %s for reading\n",cmd_args[0]);
			goto fail;			
		}

		if(fseek(bfp, 0, SEEK_END))
		{
			fprintf(stderr, "Cannot seek to the end of file %s\n",cmd_args[0]);
			goto fail;			
		}
		
		num_bytes = ftell(bfp);
		rewind(bfp);
		
		if(NULL == (bfile_buff = (BYTE *) malloc(num_bytes)))
		{
			fprintf(stderr, "Failed to allocate memory\n");
			goto fail;			
		}
		
		if(!fread(bfile_buff,1,num_bytes,bfp))
		{
			fprintf(stderr, "Cannot read from file %s\n",cmd_args[1]);
			goto fail;			
		}

		hob = (struct hobeta_header *)bfile_buff;
		if(hobeta_mode && (TO_WORD(hob->sec_bytes_h, hob->sec_bytes_l) < (num_bytes - sizeof(struct hobeta_header))))
		{
			fprintf(stderr, "Invalid hobeta file\n");
			goto fail;			
		}
		
		if(cmd_args_num > 1)
		{
			if(!trd_parse_fname(cmd_args[1],fname_ext_separator, with_del_files, &trd_filename))
			{
				fprintf(stderr, "Invalid TRDOS filename given: %s\n",cmd_args[1]);
				goto fail;	
			}
		}
		else if(hobeta_mode)
		{
			memcpy(&trd_filename, bfile_buff, sizeof(struct trd_fname));
		}
		else
		{
			if(!trd_parse_fname(cmd_args[0],fname_ext_separator, with_del_files, &trd_filename))
			{
				fprintf(stderr, "Cannot convert filename to TRDOS format: %s\n",cmd_args[0]);
				goto fail;	
			}			
		}
		
		if((fnum = trd_write_file(trd, 
			hobeta_mode? (bfile_buff+sizeof(struct hobeta_header)) : bfile_buff,
			hobeta_mode? TO_WORD(hob->sec_bytes_h, hob->sec_bytes_l) : num_bytes,
			&trd_filename)) < 0 ) goto trd_fail;
		
		if(hobeta_mode)
		{
			pcat = trd_get_cat_entry_ref(trd, fnum);
			assert(pcat != NULL);
			
			pcat->start_l = hob->start_l;
			pcat->start_h = hob->start_h;
			pcat->length_l = hob->length_l;
			pcat->length_h = hob->length_h;
		}
		
		if(!trd_img_write(trd, trd_fname)) goto trd_fail;
		
		free(bfile_buff);
		fclose(bfp);			
	}
	else if(!strcmp(command, "fread")) /*fread <fname> [bin_fname] : copy file from trd to binary*/
	{
		if(cmd_args_num < 1) goto cmd_args_fail;
		
		if(!trd_parse_fname(cmd_args[0],fname_ext_separator, with_del_files, &trd_filename))
		{
			fprintf(stderr, "Invalid TRDOS filename given: %s\n",cmd_args[0]);
			goto fail;	
		}			

		if(NULL == (pcat = trd_find_cat_entry_ref(trd, &trd_filename, with_del_files, &is_last_file)))
		{
			fprintf(stderr, "File '%s' not found\n", cmd_args[0]);
			goto fail;			
		}
		
		if(use_cat_bytelen)
		{
			if(pcat->filename.ext == 'B') num_bytes = TO_WORD(pcat->start_h, pcat->start_l); /*for BASIC full length is in 'start' field*/
			else num_bytes = TO_WORD(pcat->length_h, pcat->length_l);
			num_sectors = ceil((float)num_bytes / 0x100);
		}
		else num_bytes = (num_sectors = pcat->sectors) * 0x100;
	
		if(NULL == (bfile_buff = (BYTE *) malloc(num_sectors * 0x100)))
		{
			fprintf(stderr, "Failed to allocate memory\n");
			goto fail;			
		}
		memset(bfile_buff,0,num_bytes);
		
		if(!trd_read_sectors(trd, pcat->start_track, pcat->start_sector, num_sectors, bfile_buff)) goto trd_fail;

		if(cmd_args_num > 1) fname = cmd_args[1];
		else fname = cmd_args[0];
		
		if(NULL == (bfp = fopen(fname,"wb")))
		{
			fprintf(stderr, "Cannot open file %s for writing\n",fname);
			goto fail;			
		}

		if(hobeta_mode)
		{
			memset(&hob_w,0,sizeof(struct hobeta_header));
			memcpy(&(hob_w.filename),&(pcat->filename),sizeof(struct trd_fname));
			hob_w.sec_bytes_l = 0;
			hob_w.sec_bytes_h = pcat->sectors;
			hob_w.start_l = pcat->start_l;
			hob_w.start_h = pcat->start_h;
			hob_w.length_l = pcat->length_l;
			hob_w.length_h = pcat->length_h;
			
			hobeta_update_checksum(&hob_w);
			
			if(fwrite(&hob_w,1,sizeof(struct hobeta_header),bfp) != sizeof(struct hobeta_header))
			{
				fprintf(stderr, "Error writing to file %s\n",fname);
				goto fail;			
			}
		}
		
		if(fwrite(bfile_buff,1,num_bytes,bfp) != num_bytes)
		{
			fprintf(stderr, "Error writing to file %s\n",fname);
			goto fail;			
		}		

		free(bfile_buff);
		fclose(bfp);
	}
	else if(!strcmp(command, "del")) /*del <fname> : delete file from trd catalogue*/
	{
		if(cmd_args_num < 1) goto cmd_args_fail;
		
		if(!trd_parse_fname(cmd_args[0],fname_ext_separator, 0, &trd_filename))
		{
			fprintf(stderr, "Invalid TRDOS filename given: %s\n",cmd_args[0]);
			goto fail;	
		}
		
		if(!trd_delete_file(trd, &trd_filename)) goto trd_fail;
		if(!trd_img_write(trd, trd_fname)) goto trd_fail;
	}
	else if(!strcmp(command, "ren")) /*ren <old_fname> <new_fname> : rename file in trd catalogue*/
	{
		if(cmd_args_num < 2) goto cmd_args_fail;
		
		if(!trd_parse_fname(cmd_args[0],fname_ext_separator, with_del_files, &trd_filename))
		{
			fprintf(stderr, "Invalid TRDOS filename given: %s\n",cmd_args[0]);
			goto fail;	
		}
		if(!trd_parse_fname(cmd_args[1],fname_ext_separator, with_del_files, &trd_filename2))
		{
			fprintf(stderr, "Invalid TRDOS filename given: %s\n",cmd_args[1]);
			goto fail;	
		}
		
		if(!trd_rename_file(trd, &trd_filename, &trd_filename2, with_del_files)) goto trd_fail;
		
		if(!trd_img_write(trd, trd_fname)) goto trd_fail;
	}	
	else if(!strcmp(command, "move")) /*pack disc space (as in TRDOS 'move' command)*/
	{
		trd_move(trd);
		
		if(!trd_img_write(trd, trd_fname)) goto trd_fail;
	}
	else if(!strcmp(command, "fprop")) /*fprop <fname> <property_name> <property value>: change file property*/
	{
		if(cmd_args_num < 3) goto cmd_args_fail;
		
		if(!trd_parse_fname(cmd_args[0],fname_ext_separator, with_del_files, &trd_filename))
		{
			fprintf(stderr, "Invalid TRDOS filename given: %s\n",cmd_args[0]);
			goto fail;	
		}
		
		if(NULL == (pcat = trd_find_cat_entry_ref(trd, &trd_filename, with_del_files, &is_last_file)))
		{
			fprintf(stderr, "File '%s' not found\n", cmd_args[0]);
			goto fail;			
		}
		
		if(!strcmp(cmd_args[1],"start")) /*start adress*/
		{
			ivalue = strtol(cmd_args[2],&endptr,0);
			if(*endptr != '\0' || ivalue < 0 || ivalue > 0xFFFF) goto cmd_args_fail;
			
			pcat->start_l = (ivalue & 0xFF);
			pcat->start_h = (ivalue >> 8);
		}
		else if(!strcmp(cmd_args[1],"flen")) /*file length*/
		{
			ivalue = strtol(cmd_args[2],&endptr,0);
			if(*endptr != '\0' || ivalue < 0 || ivalue > 0xFFFF) goto cmd_args_fail;
			
			pcat->length_l = (ivalue & 0xFF);
			pcat->length_h = (ivalue >> 8);
		}
		else if(!strcmp(cmd_args[1],"nsec")) /*num of sectors*/
		{
			ivalue = strtol(cmd_args[2],&endptr,0);
			if(*endptr != '\0' || ivalue < 0 || ivalue > 0xFF) goto cmd_args_fail;
			
			pcat->sectors = ivalue;
		}		
		else goto cmd_args_fail;
		
		if(!trd_img_write(trd, trd_fname)) goto trd_fail;
	}	
	else
	{	
		fprintf(stderr, "Unknown command: %s\n", command);
		goto fail;
	}
	
	if(trd != NULL) trd_img_close(trd);
	return(EXIT_SUCCESS);

trd_fail:
	if(trd != NULL && trd_last_error_msg(trd) != NULL) fprintf(stderr, "%s\n", trd_last_error_msg(trd));
fail:
	if(bfile_buff != NULL) free(bfile_buff);
	if(bfp != NULL) fclose(bfp);
	if(trd != NULL) trd_img_close(trd);
	return(1);
	
cmd_args_fail:
	if(trd != NULL) trd_img_close(trd);
	fprintf(stderr, "Invalid arguments for command '%s'\n", command);
	return(1);
	
usage:
	usage();
	return(EXIT_SUCCESS);
}
