#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>
#include <locale.h>
#include <string.h>
typedef uint16_t us;

typedef struct __attribute__((__packed__)) {
	uint8_t BS_jmpBoot[ 3 ]; // x86 jump instr. to boot code
	uint8_t BS_OEMName[ 8 ]; // What created the filesystem
	uint16_t BPB_BytsPerSec; // Bytes per Sector
	uint8_t BPB_SecPerClus; // Sectors per Cluster
	uint16_t BPB_RsvdSecCnt; // Reserved Sector Count
	uint8_t BPB_NumFATs; // Number of copies of FAT
	uint16_t BPB_RootEntCnt; // FAT12/FAT16: size of root DIR
	uint16_t BPB_TotSec16; // Sectors, may be 0, see below
	uint8_t BPB_Media; // Media type, e.g. fixed
	uint16_t BPB_FATSz16; // Sectors in FAT (FAT12 or FAT16)
	uint16_t BPB_SecPerTrk; // Sectors per Track
	uint16_t BPB_NumHeads; // Number of heads in disk
	uint32_t BPB_HiddSec; // Hidden Sector count
	uint32_t BPB_TotSec32; // Sectors if BPB_TotSec16 == 0
	uint8_t BS_DrvNum; // 0 = floppy, 0x80 = hard disk
	uint8_t BS_Reserved1; //
	uint8_t BS_BootSig; // Should = 0x29
	uint32_t BS_VolID; // 'Unique' ID for volume
	uint8_t BS_VolLab[ 11 ]; // Non zero terminated string
	uint8_t BS_FilSysType[ 8 ]; // e.g. 'FAT16 ' (Not 0 term.)
} BootSector;

typedef struct __attribute__((__packed__)) {
	uint8_t DIR_Name[ 11 ]; // Non zero terminated string
	uint8_t DIR_Attr; // File attributes
	uint8_t DIR_NTRes; // Used by Windows NT, ignore
	uint8_t DIR_CrtTimeTenth; // Tenths of sec. 0...199
	uint16_t DIR_CrtTime; // Creation Time in 2s intervals
	uint16_t DIR_CrtDate; // Date file created
	uint16_t DIR_LstAccDate; // Date of last read or write
	uint16_t DIR_FstClusHI; // Top 16 bits file's 1st cluster
	uint16_t DIR_WrtTime; // Time of last write
	uint16_t DIR_WrtDate; // Date of last write
	uint16_t DIR_FstClusLO; // Lower 16 bits file's 1st cluster
	uint32_t DIR_FileSize; // File size in bytes
} Directory;

typedef struct __attribute__((__packed__)) {
	uint8_t LDIR_Ord; // Order/ position in sequence/ set
	uint8_t LDIR_Name1[ 10 ]; // First 5 UNICODE characters
	uint8_t LDIR_Attr; // = ATTR_LONG_NAME (xx001111)
	uint8_t LDIR_Type; // Should = 0
	uint8_t LDIR_Chksum; // Checksum of short name
	uint8_t LDIR_Name2[ 12 ]; // Middle 6 UNICODE characters
	uint16_t LDIR_FstClusLO; // MUST be zero
	uint8_t LDIR_Name3[ 4 ]; // Last 2 UNICODE characters
} LDirectory;

//// Global Data ////
const int TERMINATING_CLUSTER = 0xfff8;
BootSector* boot_sector;
us* fat;
Directory* root_directory;
LDirectory* long_root_directory;
int* long_relation_to_short;
//// Global Data ////


//////////////////// Task 1 (Reading Data From The File into a Memory Location) ////////////////////
int load_memory(char* file_name, void* memory_location, int size, int offset) {
	int fp = open(file_name, O_RDONLY);
	int lseek_val = lseek(fp, offset, SEEK_SET);
	int read_val = read(fp, memory_location, size);
	int close_val = close(fp);

	// printf("\n##########\n");
	// printf("Size To Read = %d\n", size);
	// printf("Bytes Read = %d\n", read_val);
	// printf("Errors in LSeek = %d\n", lseek_val);
	// printf("Errors in Close = %d\n", close_val);
	// printf("##########\n\n");
	return read_val;
}


//////////////////// Task 2 (Loading & Printing the Boot Sector) ////////////////////
BootSector* init_boot_sector(char* file_name) {
	BootSector* boot_sector = (BootSector*) malloc(sizeof(BootSector));
	load_memory(file_name, boot_sector, sizeof(BootSector), 0);

	return boot_sector;
}

void display_boot_sector(BootSector* boot_sector) {
	printf("\n##########\n");
	printf("BPB_BytsPerSec Bytes Per Sector = %d\n", boot_sector->BPB_BytsPerSec);
	printf("BPB_SecPerClus Sectors Per Cluster = %d\n", boot_sector->BPB_SecPerClus);
	printf("BPB_RsvdSecCnt Reserved Sector Count = %d\n", boot_sector->BPB_RsvdSecCnt);
	printf("BPB_NumFATs Number of copies of FAT = %d\n", boot_sector->BPB_NumFATs);
	printf("BPB_RootEntCnt Size of Root Directory = %d\n", boot_sector->BPB_RootEntCnt);
	printf("BPB_TotSec16 Number of Sectors = %d\n", boot_sector->BPB_TotSec16);
	printf("BPB_FATSz16 Number of Sectors in File Allocation Table (FAT) = %d\n", boot_sector->BPB_FATSz16);
	printf("BPB_TotSec32 Sectors if BPB_TotSec16 == 0: %d\n", boot_sector->BPB_TotSec32);
	printf("BS_VolLab Non zero terminated string = %.11s\n", boot_sector->BS_VolLab);
	printf("##########\n\n");
}


//////////////////// Task 3 (Loading the First FAT Array) ////////////////////
void init_fat() {
	//Total Size of FAT (Sector Count * Bytes per Sec)) / Divide by the size of an uint16_t
	int fat_byte_size = boot_sector->BPB_FATSz16 * boot_sector->BPB_BytsPerSec;
	int entries = fat_byte_size / sizeof(uint16_t);
	int sz_of_reserved_sectors = boot_sector->BPB_RsvdSecCnt * boot_sector->BPB_BytsPerSec;
	
	fat = (us*) malloc(fat_byte_size);
	for (int i = 0; i < entries; i++)
		load_memory("fat16.img", &fat[i], sizeof(us), sz_of_reserved_sectors + (i * sizeof(us)));
	
	// Printing the First 100 Entries int he FAT Array
	for (int i = 0; i < 100; i++)
		printf("%d: %hu\n", i, fat[i]);
}

void display_cluster_sequence(us start_cluster) {
	printf("======= Cluster Sequence =======\n");

	int current_cluster = start_cluster - 2; // becaue the cluster numbers start from 2 in the FAT Array, so all the indices are shifted left 2 places
	while (current_cluster < TERMINATING_CLUSTER) {
		printf("Current Cluster = %d\n", current_cluster);
		current_cluster = fat[current_cluster];
	}

	printf("======= Cluster Sequence Finished =======\n\n");
}


//////////////////// Task 4 (Loading & Displaying Root Directory Entries) ////////////////////
int dir_type(int dir_attr) {
	int dir_test = 1, volume_test = 1;
	volume_test <<= 3;
	dir_test <<= 4;

	bool is_dir = dir_attr & dir_test;
	bool is_volume = dir_attr & volume_test;

	if (!is_dir && !is_volume) return 0; // Means a normal File
	else if (is_volume && !is_dir) return 1; // Means a Disk/Volume
	else if (is_dir && !is_volume) return 2; // Means a Folder

	return -1; // Means a Value & a Folder at the Same Time !!!
}

void display_date(int date) {
	int day_mask = 31;
	int day = date;
	day &= day_mask;

	int month_mask = 15;
	int month = date;
	month >>= 5;
	month &= month_mask;

	int year_mask = 127; // not really required
	int year = date;
	year >>= 9;

	char space = ' ';
	printf("%9c %02d/%02d/%04d", space, month, day, year + 1980);
	printf("|");
}

void display_time(int time) {
	int seconds_mask = 31;
	int seconds = time;
	seconds &= seconds_mask;
	seconds *= 2;

	int minutes_mask = 63;
	int minutes = time;
	minutes >>= 5;
	minutes &= minutes_mask;

	int hours_mask = 31; // not really required
	int hours = time;
	hours >>= 11;

	char space = ' ';
	printf("%11c %02hu:%02hu:%02hu", space, hours, minutes, seconds);
	printf("|");
}

void display_attributes(int attr) {
	printf("      ");

	if (attr>>5 & 1) printf("A");
	else printf("-");

	if (attr>>4 & 1) printf("D");
	else printf("-");

	if (attr>>3 & 1) printf("V");
	else printf("-");

	if (attr>>2 & 1) printf("S");
	else printf("-");

	if (attr>>1 & 1) printf("H");
	else printf("-");

	if (attr & 1) printf("R");
	else printf("-");

	printf("|");

	int typeof_dir = dir_type(attr);	
	if (typeof_dir == 0) printf("     Normal File");
	else if (typeof_dir == 1) printf("     Disk/Volume");
	else if (typeof_dir == 2) printf("          Folder");
	else printf("Can't Identify the Type!!!");
	printf("|");
}

int get_cluster_number(int high_cluster, int low_cluster) {
	int cluster = (high_cluster << 16) | low_cluster;
	return cluster;
}

void display_dir_name(char* name, int dir_attr) {
	if (name[0] == 0) {printf("    Invalid"); return;}
	if (name[0] == 0xE5) {printf("    ignored"); return;}

	int count_char = 0;
	for (int i = 0; i < 11; i++) {
		if (name[i] <= 0x20 || name[i] == 0) continue;
 		if (i == 8 && dir_type(dir_attr) == 0) {printf("."); count_char++;}

 		printf("%c", name[i]);
 		count_char++;
	}

	count_char = 12 - count_char;
	for (int i = 0; i < count_char; i++)
		printf(" ");
	printf("|");
}

void display_directory(Directory dir) {
	// Displaying Dir Cluster Number //
	printf("|");
	printf("%16hu |", get_cluster_number(dir.DIR_FstClusHI, dir.DIR_FstClusLO));
	
	// Displaying Data & Time of Last Write //
	display_time(dir.DIR_WrtTime);
	display_date(dir.DIR_WrtDate);
	
	// Displaying the Dir Attributes //
	display_attributes(dir.DIR_Attr);

	// Displaying the Dir Size //
	printf("%13hu", dir.DIR_FileSize);
	printf("|");

	// Displaying the Dir Name //
	display_dir_name(dir.DIR_Name, dir.DIR_Attr);

	// Displaying if the Dir should be ignored for now //
	if (dir.DIR_Attr == 15) printf("%13ctrue|", ' ');
	else printf("%12cfalse|", ' ');

	printf("\n----------------------------------------------------------------------------------------------------------------------------------------------------\n");
}

void display_root_dir() {
	printf("===================================================================================================================================================\n");
	printf("|Dir Number |Starting Cluster | Last Modified Time | Last Modified Date | Attributes | Directory Type | File Length |  Filename  | Ignored for now |\n");
	printf("===================================================================================================================================================\n");

	// Displaying Root Direcotry //
	for (int i = 0; i < boot_sector->BPB_RootEntCnt; i++) {
		if (root_directory[i].DIR_Name[0] == 0) break;
		
		printf("|%11d", i + 1);
		display_directory(root_directory[i]);
	}
}

void init_root_dir() {
	int root_dir_sz = boot_sector->BPB_RootEntCnt * sizeof(Directory);
	root_directory = (Directory*) malloc(root_dir_sz);
	long_relation_to_short = (int*) malloc(boot_sector->BPB_RootEntCnt * sizeof(int));

	printf("Size = %d\n", root_dir_sz);

	int offset_root_dir = (boot_sector->BPB_RsvdSecCnt + (boot_sector->BPB_NumFATs * boot_sector->BPB_FATSz16)) * boot_sector->BPB_BytsPerSec;
	int mem = load_memory("fat16.img", root_directory, root_dir_sz, offset_root_dir);
	printf("Mem = %d\n", mem);

	for (int i = 0; i < boot_sector->BPB_RootEntCnt; i++)
		long_relation_to_short[i] = -1;
}


//////////////////// Task 5 (Prinitng the File Data) ////////////////////
char* getClusterData(int cluster_num, int required_bytes) {
		const int cluster_sz = boot_sector->BPB_SecPerClus * boot_sector->BPB_BytsPerSec;
		if (required_bytes > cluster_sz) return "\0";

		char* data_buffer = (char*) malloc(required_bytes + 1);
		int data_offset = boot_sector->BPB_RsvdSecCnt;
		data_offset += (boot_sector->BPB_NumFATs * boot_sector->BPB_FATSz16);
		data_offset *= boot_sector->BPB_BytsPerSec;
		data_offset += (boot_sector->BPB_RootEntCnt * sizeof(Directory));
		data_offset += (cluster_num * boot_sector->BPB_SecPerClus * boot_sector->BPB_BytsPerSec);

		load_memory("fat16.img", data_buffer, required_bytes, data_offset);

		data_buffer[required_bytes] = '\0';
		return data_buffer;
}

void display_file(int dir_pos) {
	Directory dir = root_directory[dir_pos];
	if (dir_type(dir.DIR_Attr) != 0) {printf("Not a File\n"); return;}

	int total_file_sz = dir.DIR_FileSize;
	const int cluster_sz = boot_sector->BPB_SecPerClus * boot_sector->BPB_BytsPerSec;
	int start_cluster = get_cluster_number(dir.DIR_FstClusHI, dir.DIR_FstClusLO) - 2;
	
	while (start_cluster < TERMINATING_CLUSTER) {
		char* file_contents;
		if (total_file_sz >= cluster_sz) {file_contents = getClusterData(start_cluster, cluster_sz); total_file_sz -= cluster_sz;}
		else file_contents = getClusterData(start_cluster, total_file_sz);

		printf("%s", file_contents);

		// Goging to the next cluster
		start_cluster = fat[start_cluster];
	}
}

int menu() {
	int choice = -2;
	while (choice == -2) {
		printf("Please Choose a File you want to choose\n");
		printf("To Exit Please Input -1: ");
		scanf("%d", &choice);
	}

	return choice;
}

void user_interface() {
	while (true) {
		int choice = menu();
		if (choice == -1) break;
		display_file(choice);
	}
}


//////////////////// Task 6 (Handlng Long File Names) ////////////////////
// Useful Link Explaining How Long File Names Work: https://www.ntfs.com/fat-filenames.htm
void init_long_dir() {
	long_root_directory = (LDirectory*) malloc(sizeof(LDirectory) * boot_sector->BPB_RootEntCnt);

	int idx = 0;
	for (int i = 0; i < boot_sector->BPB_RootEntCnt; i++) {
		if (root_directory[i].DIR_Name[0] == 0) break;
		if (root_directory[i].DIR_Attr != 15) continue;

		int offset = (boot_sector->BPB_RsvdSecCnt + boot_sector->BPB_NumFATs * boot_sector->BPB_FATSz16) * boot_sector->BPB_BytsPerSec;
		offset += (i * sizeof(LDirectory));
		load_memory("fat16.img", &long_root_directory[idx], sizeof(LDirectory), offset);

		if (long_root_directory[idx].LDIR_Ord >= 0x41) {
			long_root_directory[idx].LDIR_Ord -= 0x40;
			long_relation_to_short[i + long_root_directory[idx].LDIR_Ord] = idx;
		}

		idx++;
	}
}

void display_long_root_dir() {
	printf("\n====Display Long File Names====\n");

	for (int i = 0; i < boot_sector->BPB_RootEntCnt; i++) {
		if (root_directory[i].DIR_Name[0] == 0) break;
		if (root_directory[i].DIR_Attr == 15) continue;

		// Displaying Dir Cluster Number //
		printf("|");
		printf("%16hu |", get_cluster_number(root_directory[i].DIR_FstClusHI, root_directory[i].DIR_FstClusLO));

		if (long_relation_to_short[i] != -1) {
			// Print Reverse
			int highPos = long_relation_to_short[i];
			int lowPos = long_root_directory[highPos].LDIR_Ord + highPos;
			for (int k = lowPos - 1; k >= highPos; k--) {
				for (int j = 0; j < 10; j++) {
					if (long_root_directory[k].LDIR_Name1[j] == 0) continue;
					printf("%c", long_root_directory[k].LDIR_Name1[j]);
				}

				for (int j = 0; j < 12; j++) {
					if (long_root_directory[k].LDIR_Name2[j] == 0) continue;
					printf("%c", long_root_directory[k].LDIR_Name2[j]);
				}

				for (int j = 0; j < 4; j++) {
					if (long_root_directory[k].LDIR_Name3[j] == 0) continue;
					printf("%c", long_root_directory[k].LDIR_Name3[j]);
				}				
			}

			printf("\n");
			continue;
		}

		for (int j = 0; j < 11; j++) {
			if (root_directory[i].DIR_Name[j] == ' ' || root_directory[i].DIR_Name[j] == '\0') continue;
			printf("%c", root_directory[i].DIR_Name[j]);
		}

		printf("\n");
	}
}


int main(int argc, char const *argv[]) {
	// printf("Fat Size = %d\n", boot_sector->BPB_FATSz16);

	// Task 2 //
	boot_sector = init_boot_sector("fat16.img");
	display_boot_sector(boot_sector);
	
	// Task 3 //
	init_fat();
	display_cluster_sequence(93); // just a random number that I've when printing the fat array

	// Task 4 //
	init_root_dir();
	display_root_dir();

	// Task 5 //
	user_interface();	

	// Task 6 //
	init_long_dir();
	display_long_root_dir();


	return 0;
}