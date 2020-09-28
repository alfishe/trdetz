#ifndef __TRD_H_INCLUDED__
#define __TRD_H_INCLUDED__

#define TRD_SIZE 655360

struct trd_context {
	BYTE trd_buff[TRD_SIZE];	
	char *last_error_msg;	
};

#ifdef __GNUC__
#pragma pack(push,1)
#else
#pragma pack(push)
#endif
#pragma pack(1)

struct trd_fname {
	BYTE name[8];
	BYTE ext;
};

struct trd_cat_entry {
	struct trd_fname filename;
	BYTE start_l;
	BYTE start_h;
	BYTE length_l;
	BYTE length_h;
	BYTE sectors;
	BYTE start_sector;
	BYTE start_track;
};

struct trd_disc_info {
	BYTE DCU_f_l;
	BYTE DCU_f_h;
	BYTE next_free_sector;
	BYTE next_free_track;
	BYTE disc_type;
	BYTE files;
	BYTE free_sectors_l;
	BYTE free_sectors_h;
	BYTE sectors_on_track;
	BYTE zero1 [2];
	BYTE spc1 [9];
	BYTE zero2;
	BYTE deleted_files;
	BYTE disc_name[8];
};
#pragma pack(pop)

#ifndef __TRD_H_SELF_INCLUDE

extern char *trd_last_error_msg(struct trd_context *trd);

extern BYTE trd_read_byte(struct trd_context *trd, unsigned offset);

extern void trd_write_byte(struct trd_context *trd, unsigned offset, BYTE data);

extern struct trd_disc_info *trd_get_disc_info_ref(struct trd_context *trd);

/*get pointer to catalogue entry <entry_num> */
extern struct trd_cat_entry *trd_get_cat_entry_ref(struct trd_context *trd, unsigned entry_num);

/*get pointer to catalogue entry for file <fname>. returns NULL if not found*/
extern struct trd_cat_entry *trd_find_cat_entry_ref(struct trd_context *trd, struct trd_fname *fname, char with_del_files, int *is_last_file);

extern int trd_read_sectors(struct trd_context *trd, unsigned start_track, unsigned start_sector, unsigned sector_count, BYTE *output);

extern int trd_write_sectors(struct trd_context *trd, unsigned start_track, unsigned start_sector, unsigned sector_count, BYTE *data);

extern struct trd_context *trd_img_create(void);

extern int trd_img_read(struct trd_context *trd, char *fname);

extern int trd_img_write(struct trd_context *trd, char *fname);

extern void trd_img_close(struct trd_context *trd);

extern void trd_move(struct trd_context *trd);

/*write binary data <bfile_buff> of <num_bytes> as file <fname>*/
/*returns number of new file entry in catalogue or -1 when error*/
extern int trd_write_file(struct trd_context *trd, BYTE *bfile_buff, int num_bytes, struct trd_fname *fname);

extern int trd_delete_file(struct trd_context *trd, struct trd_fname *fname);

extern int trd_rename_file(struct trd_context *trd, struct trd_fname *current_fname, struct trd_fname *new_fname, int with_del_files);

extern int trd_fname_equal(struct trd_fname *fname1, struct trd_fname *fname2);

extern int trd_parse_fname(char *filename, char fname_ext_separator, char with_del_files, struct trd_fname *fname);


#endif

#endif
