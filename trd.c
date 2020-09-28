#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "defines.h"
#define __TRD_H_SELF_INCLUDE
#include "trd.h"

char *trd_last_error_msg(struct trd_context *trd)
{
	return(trd->last_error_msg);	
}

BYTE trd_read_byte(struct trd_context *trd, unsigned offset)
{
	if(offset >= TRD_SIZE) return(0);
	return((trd->trd_buff)[offset]);	
}

void trd_write_byte(struct trd_context *trd, unsigned offset, BYTE data)
{
	if(offset >= TRD_SIZE) return;
	(trd->trd_buff)[offset] = data;
	return;	
}

struct trd_disc_info *trd_get_disc_info_ref(struct trd_context *trd)
{
	return((struct trd_disc_info *)((trd->trd_buff)+0x100*8+0xDF));
}

struct trd_cat_entry *trd_get_cat_entry_ref(struct trd_context *trd, unsigned entry_num)
{
	struct trd_cat_entry *pcat=NULL;
	if(entry_num < 128) pcat = ((struct trd_cat_entry *)(trd->trd_buff)) + entry_num;	
	return(pcat);
}

int trd_read_sectors(struct trd_context *trd, unsigned start_track, unsigned start_sector, unsigned sector_count, BYTE *output)
{
	unsigned offset, to_read;
	
	if(start_track > 159 || start_sector > 15)
	{
		trd->last_error_msg = "Track/sector out of disc bounds";
		goto trd_read_sectors_fail;		
	}
	
	offset = (start_track << 12) + (start_sector << 8);
	to_read = sector_count*0x100;
	if((offset+to_read) > TRD_SIZE)
	{
		trd->last_error_msg = "Track/sector out of disc bounds";
		goto trd_read_sectors_fail;		
	}
		
	if(to_read) memcpy(output,(trd->trd_buff)+offset,to_read);
	
	return(1);
	
trd_read_sectors_fail:
	return(0);
}

int trd_write_sectors(struct trd_context *trd, unsigned start_track, unsigned start_sector, unsigned sector_count, BYTE *data)
{
	unsigned offset, to_write;
		
	if(start_track > 159 || start_sector > 15)
	{
		trd->last_error_msg = "Track/sector out of disc bounds";
		goto trd_write_sectors_fail;
	}
	
	offset = (start_track << 12) + (start_sector << 8);
	to_write = sector_count*0x100;
	if((offset+to_write) > TRD_SIZE)
	{
		trd->last_error_msg = "Track/sector out of disc bounds";
		goto trd_write_sectors_fail;		
	}		
		
	if(to_write) memcpy((trd->trd_buff)+offset,data,to_write);
	
	return(1);
	
trd_write_sectors_fail:
	return(0);
}

extern struct trd_context *trd_img_create(void)
{
	struct trd_context *trd;
	
	trd = (struct trd_context *)malloc(sizeof(struct trd_context));
	if(trd ==NULL) return(NULL);
	
	memset(trd->trd_buff,0,TRD_SIZE);
	trd->last_error_msg = NULL;	
	return(trd);
}

extern int trd_img_read(struct trd_context *trd, char *fname)
{
	FILE *fp;
	
	if(NULL == (fp = fopen(fname,"rb")))
	{
		trd->last_error_msg = "Cannot open trd file for reading";
		return(0);
	}		
	else
	{
		fread(trd->trd_buff,1,TRD_SIZE,fp);
		fclose(fp);
	}
	
	return(1);
}

int trd_img_write(struct trd_context *trd, char *fname)
{
	FILE *fp;
	
	if(NULL == (fp = fopen(fname,"wb")))
	{
		trd->last_error_msg = "Cannot open trd file for writing";
		goto trd_img_fwrite_fail;		
	}
	
	if(TRD_SIZE != fwrite(trd->trd_buff,1,TRD_SIZE,fp)) 
	{
		trd->last_error_msg = "Cannot write data to trd file";
		goto trd_img_fwrite_fail;		
	}
	
	fclose(fp);
	
	return(1);
	
trd_img_fwrite_fail:
	if(fp != NULL) fclose(fp);
	return(0);
}

void trd_img_close(struct trd_context *trd)
{
	free(trd);
	return;
}

void trd_move(struct trd_context *trd)
{
	struct trd_cat_entry *pcat;
	int i, num_files, move_by_sec=0, nloc, tmpw;
	BYTE *file_offset;
	BYTE *trd_b;
	struct trd_disc_info *dinfo = NULL;
	int moved=0;
	trd_b = trd->trd_buff;
	
	num_files=trd_read_byte(trd,(0x100*8)+0xE4);
	for(i=0; i < (num_files-moved);)
	{
		pcat=trd_get_cat_entry_ref(trd,i);
		if(move_by_sec)
		{
			nloc = (int)((pcat->start_track << 4) + pcat->start_sector) - move_by_sec;
			assert(nloc >= 0x0F);
			/*printf("[mov = %d sec] track= %d -> %d     sector = %d -> %d\n", move_by_sec,  pcat->start_track,(nloc >> 4),pcat->start_sector,(nloc & 0x0F));*/			
			pcat->start_track = (nloc >> 4);
			pcat->start_sector = (nloc & 0x0F);
		}
		
		if(!((pcat->filename.name)[0]) || ((pcat->filename.name)[0]) == 0x01)
		{
			move_by_sec+=pcat->sectors;
			
			file_offset = (trd_b + (pcat->start_track << 12) + (pcat->start_sector << 8));
			memcpy(file_offset, file_offset+(pcat->sectors * 0x100), TRD_SIZE - ((file_offset+(pcat->sectors * 0x100)) - trd_b)); /*move file data*/

			memcpy(pcat, pcat+1, 0x100*8 - (((BYTE *)pcat) - trd_b)); /*move catalogue entries*/
			++moved;
		}
		else ++i;
	}

	/*update 8th sector of 0th track*/
	
	dinfo=trd_get_disc_info_ref(trd);
	
	tmpw = (int)((pcat->start_track << 4) + pcat->start_sector + pcat->sectors);
	dinfo->next_free_track = (tmpw >> 4);
	dinfo->next_free_sector = (tmpw & 0x0F);
	
	tmpw = TO_WORD(dinfo->free_sectors_h,dinfo->free_sectors_l);
	tmpw += move_by_sec;
	dinfo->free_sectors_h = (tmpw >> 8);
	dinfo->free_sectors_l = (tmpw & 0xFF);
	
	dinfo->files -= moved; /*num. of files incl. deleted*/
	dinfo->deleted_files = 0; /*num. of deleted files*/
	
	return;	
}

int trd_fname_equal(struct trd_fname *fname1, struct trd_fname *fname2)
{
	if((!(fname1->name[0]) || fname1->name[0] == 0x01) && (!(fname2->name[0]) || fname2->name[0] == 0x01))
	{
		if(memcmp(&(fname1->name[1]),&(fname2->name[1]),7)) return(0);
		if(fname1->ext != fname2->ext) return(0);
		return(1);
	}
	
	return(!memcmp(fname1,fname2,sizeof(struct trd_fname)));
}

int trd_parse_fname(char *filename, char fname_ext_separator, char with_del_files, struct trd_fname *fname)
{
	char *sep;
	int len;
	
	if(!*filename || (len=strlen(filename)) > 10) return(0);
	
	memcpy(fname->name,"        ",8);
	fname->ext=' ';
	sep=strrchr(filename,fname_ext_separator);
	if(sep != NULL && *(sep+1))
	{
		fname->ext=*(sep+1);
		len=sep-filename;
	}
	
	memcpy(fname->name,filename,(len > 8)? 8 : len);
	
	if(with_del_files && *filename == with_del_files) fname->name[0]=0x01;
	
	return(1);
}

struct trd_cat_entry *trd_find_cat_entry_ref(struct trd_context *trd, struct trd_fname *fname, char with_del_files, int *is_last_file)
{
	struct trd_cat_entry *pcat;
	int i;
	
	for(i=0, pcat=(struct trd_cat_entry *)(trd->trd_buff); i< 128; ++i, ++pcat)
	{
		if(((!(pcat->filename.name[0]) || pcat->filename.name[0] == 0x01) && !with_del_files)) continue;
			
		if(trd_fname_equal(fname, &(pcat->filename))) 
		{
			if(!(((pcat+1)->filename).name)[0] || i == 127) *is_last_file=1;
			else *is_last_file=0;
				
			return(pcat);
		}
	}	
	
	return(NULL);
}

int trd_write_file(struct trd_context *trd, BYTE *bfile_buff, int num_bytes, struct trd_fname *fname)
{
	int fnum, sector=0, track=0, num_sectors=0, tmpw;
	struct trd_cat_entry *pcat = NULL;
	struct trd_disc_info *dinfo = NULL;
	BYTE *trd_b;
	trd_b = trd->trd_buff;
	
	for(fnum=0, pcat=(struct trd_cat_entry *)trd_b; fnum< 128; ++fnum, ++pcat)
	{
		if(!(pcat->filename.name[0])) break;
			
		sector = ((pcat->start_sector + pcat->sectors) & 0x0F);
		track = pcat->start_track + ((pcat->start_sector + pcat->sectors) >> 4);
	}			
	
	if(fnum >= 128)
	{
		trd->last_error_msg = "File limit of 128 reached";
		goto trd_writefile_fail;			
	}
	
	num_sectors = ceil((float)num_bytes / 0x100);
	
	if(num_sectors > 0x100)
	{
		trd->last_error_msg = "Too many sectors";
		goto trd_writefile_fail;
	}		
	
	if(num_sectors > *(trd_b+(0x100*8)+0xE5))
	{
		trd->last_error_msg = "Not enough free sectors on disc";
		goto trd_writefile_fail;
	}
	
	if(!trd_write_sectors(trd, track, sector, num_sectors, bfile_buff)) goto trd_writefile_fail;
	
	/*form trd_cat_entry*/
	
	memset(pcat,0,sizeof(struct trd_cat_entry));
	memcpy(&(pcat->filename), fname, sizeof(struct trd_fname));
	
	pcat->sectors = num_sectors;
	pcat->start_sector = sector;
	pcat->start_track = track;
	/*TODO: support for BASIC program length*/
	pcat->length_l = (num_bytes & 0xFF);
	pcat->length_h = (num_bytes >> 8);

	/*update 8th sector of 0th track*/
	
	dinfo=trd_get_disc_info_ref(trd);

	tmpw = ((pcat->start_track << 4) + pcat->start_sector + pcat->sectors);
	dinfo->next_free_track = (tmpw >> 4);
	dinfo->next_free_sector = (tmpw & 0x0F);

	tmpw = TO_WORD(dinfo->free_sectors_h,dinfo->free_sectors_l);
	tmpw -= pcat->sectors;
	dinfo->free_sectors_h = (tmpw >> 8);
	dinfo->free_sectors_l = (tmpw & 0xFF);

	++(dinfo->files);

	return(fnum);
	
trd_writefile_fail:
	return(-1);
}

int trd_rename_file(struct trd_context *trd, struct trd_fname *current_fname, struct trd_fname *new_fname, int with_del_files)
{
	struct trd_cat_entry *pcat;
	int is_last_file;
	struct trd_disc_info *dinfo = NULL;
	
	if(NULL == (pcat = trd_find_cat_entry_ref(trd, current_fname, with_del_files, &is_last_file)))
	{
		trd->last_error_msg = "No such file to rename";
		goto trd_rename_file_fail;			
	}
	
	if(!(pcat->filename.name[0]) || pcat->filename.name[0]==0x01) /*undelete file?*/
	{
		dinfo=trd_get_disc_info_ref(trd);
		--(dinfo->deleted_files);
	}
	
	memcpy(&(pcat->filename),new_fname,sizeof(struct trd_fname));
	
	return(1);	
		
trd_rename_file_fail:
	return(0);
}

int trd_delete_file(struct trd_context *trd, struct trd_fname *fname)
{
	struct trd_disc_info *dinfo = NULL;
	int tmpw,is_last_file;
	struct trd_cat_entry *pcat;
	BYTE *trd_b;
	trd_b = trd->trd_buff;
	
	if(NULL == (pcat = trd_find_cat_entry_ref(trd, fname, 0, &is_last_file))) goto trd_delete_file_fail;
	
	dinfo=trd_get_disc_info_ref(trd);
	
	if(pcat->filename.name[0] && pcat->filename.name[0]!=0x01)
	{
		pcat->filename.name[0] = (is_last_file? 0x00: 0x01);
	
		/*update sys.sector*/
		if(is_last_file)
		{
			dinfo->next_free_sector = pcat->start_sector;
			dinfo->next_free_track = pcat->start_track;
			
			tmpw = TO_WORD(dinfo->free_sectors_h,dinfo->free_sectors_l);
			tmpw += pcat->sectors;
			dinfo->free_sectors_h = (tmpw >> 8);
			dinfo->free_sectors_l = (tmpw & 0xFF);
			
			--(dinfo->files);
		}
		else ++(dinfo->deleted_files);
		
		return(1);
	}
	
trd_delete_file_fail:	
	trd->last_error_msg = "No such file to delete";
	return(0);
}

