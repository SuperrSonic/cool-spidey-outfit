#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <ogc/machine/processor.h>
#include <fat.h>

#ifdef HW_RVL
#include <sdcard/wiisd_io.h>
#else
#include <sdcard/gcsd.h>
#endif

#include <asndlib.h>
#include "sound.h"
#include "vi_encoder.h"

#include <unistd.h>
#include <ogc/lwp_watchdog.h> // for timer stuff
#include <string.h>
#include <malloc.h> // for memalign

// for directory parsing and low-level file I/O
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include "input.h"

#include "Spidey2007_bin.h"
#include "Spidey2007SHADE_bin.h"
#include "miguel_black_bin.h"
#include "miguel_blackSHADE_bin.h"
#include "miguel_blue_bin.h"
#include "miguel_blueSHADE_bin.h"
#include "OG_spidey_bin.h"
#include "OG_spideySHADE_bin.h"
#include "PSone_Spidey_bin.h"
#include "SilverSpidey_bin.h"
#include "PinkSpidey_bin.h"
#include "GreenSpidey_bin.h"
#include "Venom_Spidey_bin.h"
#include "Venom_SpideySHADE_bin.h"

char iso_path[256] = {0};
char iso_name[64] = {0};
char mcd_path[256] = {0};
char jingle_path[256] = {0};
//static bool playMusic = false;
static bool showInfo = false;

// configurable settings
static bool deflickerPatch = false;
static unsigned forceAddress = 0;
static bool searchFST = true;

// basic video setup
static void initialize(GXRModeObj *rmode)
{
	static void *xfb = NULL;

	if (xfb)
		free(MEM_K1_TO_K0(xfb));

	xfb = SYS_AllocateFramebuffer(rmode);
	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
	xfb = MEM_K0_TO_K1(xfb);
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(TRUE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	VIDEO_WaitVSync();
}

void setOption(char* key, char* valuePointer){
	bool isString = valuePointer[0] == '"';
	//char value = 0;
	
	if(isString) {
		char* p = valuePointer++;
		while(*++p != '"');
		*p = 0;
	} //else
		//value = atoi(valuePointer);
	
	unsigned int i = 0;
	for(i=0; i<8; i++){
		if(!strcmp("iso-path", key)) {
			sprintf(iso_path, "%s", valuePointer);
		} else if(!strcmp("--ask-deflicker", key)) {
			deflickerPatch = true;
		} else if(!strcmp("--no-search", key)) {
			searchFST = false;
		} else if(!strcmp("addr-pak", key)) {
			forceAddress = strtol(valuePointer, NULL, 16);
		} /*else if(!strcmp("suit-path", key)) {
			sprintf(mcd_path, "%s", valuePointer);
		} */
		//break;
	}
}

void handleConfigPair(char* kv){
	char* vs = kv;
	while(*vs != ' ' && *vs != '\t' && *vs != ':' && *vs != '=')
			++vs;
	*(vs++) = 0;
	while(*vs == ' ' || *vs == '\t' || *vs == ':' || *vs == '=')
			++vs;

	setOption(kv, vs);
}

u8* mainMusic = NULL;
char demoCredit[] = ">Standalone Edition by Diego A. 06/30/2023";
//static u64 timerDone = 0;


//Spidey Stuff
//0x150C120 = Design banner, 128x128 CMPR 0x2000
//0x1508CE0 = Good Shot banner, 128x128 CMPR (fmt 14) 0x2000
//0x8138720 = spiderman suit 512x256 C4 (fmt 8) 16 color palette, size 0x10000
//0x81304C0 = spiderman suit shade 512x128 C4 (fmt 8) size = 0x8000

static char SP_QUOTES[10][48] =
{
	"\"I don't have any pockets...\"",
	"\"Nobody will get here in time to saveUJONAH!\"",
	"\"I'm gonna put some dirt in your eye.\"",
	"\"Let's rocket! M-E-G-A\"",
	"\"DRAGONZORD!\"",
	"\"Web-slinger Barbie\nhits with dat ddududdu!\"",
	"A men's cologne called THWIP?",
	"Lemme give you my card--",
	"Well we're right in the building.",
	"Looks like I have a superhero stalker."
};

static char DF_TITLES[4][16] =
{
	"Don't Patch",
	"Remove",
	"Restore",
	"Dark"
};

static char SUIT_TITLES[10][32] =
{
	"SPIDER-MAN 2000",
	"SPIDER-MAN 2000 Black Suit",
	"2007 Black Suit",
	"2099",
	"2099 Black Suit",
	"Super Silverizer",
	"Barbie x BLACKPINK",
	"Green With Evil",
	"2004 Restore Original",
	"From TPL on Storage Device"
};

static const unsigned fstPTR = 0x424;

static const unsigned title_sz = 0x10000; //512x256 CMPR
static const unsigned tribeca_sz = 0x2000; //128x128 CMPR
static const unsigned amalga_addr_clean = 0x0D84E800; //clean ntsc-u iso
static const unsigned amalga_addr_shrunk = 0x09BA4B30; //shrunk ntsc-u iso
static const unsigned amalga_addr_nkit = 0x09BA4B34; //nkit ntsc-u iso
//PAL
static const unsigned amalga_addr_clean_pal = 0x0D84E800; //clean pal iso
static const unsigned amalga_addr_shrunk_pal = 0x0A0D80C8; //shrunk pal iso (assuming)
static const unsigned amalga_addr_nkit_pal = 0x0A0D80CC; //nkit pal iso

//amalga pak offsets
static const unsigned goodShotBnr = 0x1508CE0; //this is at Tribeca, coast side
static const unsigned designBnr = 0x150C120; //same as above
static const unsigned artBnr = 0x4F55E0; //this is behind (closest to coast) apartment village in Gramercy
static const unsigned mjBnr = 0x00; //comic shop closest to pizza parlor

static const unsigned titleScreen = 0x816C580;
static const unsigned titleMask = 0x815C560;

static const unsigned spideyShade = 0x81304C0;
static const unsigned spideyMain = 0x8138720;

static int SelectDF()
{
	u32 pressed;
	int choose = 0;
	
	printf("\n");
	
	printf("Deflicker Patch:  <%s> ", DF_TITLES[0]);
	do
	{
		VIDEO_WaitVSync();
		get_input(NULL, &pressed);
		if (!pressed)
			continue;

		if (pressed & INPUT_BUTTON_START || pressed & INPUT_BUTTON_CANCEL)
		{
			printf("\nExiting...\n");
			exit(0);
		}

		if (pressed & INPUT_BUTTON_RIGHT)
			++choose;
		else if (pressed & INPUT_BUTTON_LEFT)
			--choose;

		if(choose > 3)
			choose = 0;
		if(choose < 0)
			choose = 3;

		printf("\rDeflicker Patch:  <%s> \x1b[K", DF_TITLES[choose]);
	} while (!(pressed & INPUT_BUTTON_OK));
	
	return choose;
}

static int SelectSuit()
{
	u32 pressed;
	int choose = 0;
	
	printf("\n");
	
	printf("Select:  <%s> ", SUIT_TITLES[0]);
	do
	{
		VIDEO_WaitVSync();
		get_input(NULL, &pressed);
		if (!pressed)
			continue;

		if (pressed & INPUT_BUTTON_START || pressed & INPUT_BUTTON_CANCEL)
		{
			printf("\n%s", SP_QUOTES[7]);
			sleep(1);
			printf("\n%s\n", SP_QUOTES[8]);
			sleep(2);
			printf("\nExiting...\n");
			exit(0);
		}

		if (pressed & INPUT_BUTTON_RIGHT)
			++choose;
		else if (pressed & INPUT_BUTTON_LEFT)
			--choose;

		if(choose > 9)
			choose = 0;
		if(choose < 0)
			choose = 9;

		printf("\rSelect:  <%s> \x1b[K", SUIT_TITLES[choose]);
	} while (!(pressed & INPUT_BUTTON_OK));
	
	return choose;
}

void writeVFilter(FILE* fd)
{
	if(deflickerPatch) {
		u8 vfilter_arr[7] = {0, 0, 21, 22, 21, 0, 0};
		u8 vfilter_res[7] = {8, 8, 10, 12, 10, 8, 8};
		u8 vfilter_dim[7] = {0, 0, 21, 0, 21, 0, 0};
		int df = SelectDF();
		const int dfOffset = 0x499932;
		int dolPos = 0;
		fseek(fd, 0x420, SEEK_SET);
		fread(&dolPos, 1, 4, fd);
		fseek(fd, dolPos + dfOffset, SEEK_SET);
		if(df == 1)
			fwrite(vfilter_arr, 1, 7, fd);
		else if(df == 2)
			fwrite(vfilter_res, 1, 7, fd);
		else if(df == 3)
			fwrite(vfilter_dim, 1, 7, fd);
	}
}

void writeSuit(FILE* fd, unsigned base_amalga)
{
	int val = SelectSuit();
	if(val == 0)
	{
		//2000
		fseek(fd, base_amalga + spideyMain, SEEK_SET);
		fwrite(PSone_Spidey_bin, 1, 0x10020, fd);
		
		//probably need to restore shade
		fseek(fd, base_amalga + spideyShade, SEEK_SET);
		fwrite(OG_spideySHADE_bin, 1, 0x8020, fd);
		
		printf("\n%s\n", SP_QUOTES[1]);
	}
	else if(val == 1)
	{
		//2000 Black
		fseek(fd, base_amalga + spideyMain, SEEK_SET);
		fwrite(Venom_Spidey_bin, 1, 0x10020, fd);
		
		fseek(fd, base_amalga + spideyShade, SEEK_SET);
		fwrite(Venom_SpideySHADE_bin, 1, 0x8020, fd);
	}
	else if(val == 2)
	{
		//2007 Black
		fseek(fd, base_amalga + spideyMain, SEEK_SET);
		fwrite(Spidey2007_bin, 1, 0x10020, fd);
		
		fseek(fd, base_amalga + spideyShade, SEEK_SET);
		fwrite(Spidey2007SHADE_bin, 1, 0x8020, fd);
		
		printf("\n%s\n", SP_QUOTES[2]);
	}
	else if(val == 3)
	{
		//2099
		fseek(fd, base_amalga + spideyMain, SEEK_SET);
		fwrite(miguel_blue_bin, 1, 0x10020, fd);
		
		fseek(fd, base_amalga + spideyShade, SEEK_SET);
		fwrite(miguel_blueSHADE_bin, 1, 0x8020, fd);
	}
	else if(val == 4)
	{
		//2099 Black
		fseek(fd, base_amalga + spideyMain, SEEK_SET);
		fwrite(miguel_black_bin, 1, 0x10020, fd);
		
		fseek(fd, base_amalga + spideyShade, SEEK_SET);
		fwrite(miguel_blackSHADE_bin, 1, 0x8020, fd);
	}
	else if(val == 5)
	{
		//Silver
		fseek(fd, base_amalga + spideyMain, SEEK_SET);
		fwrite(SilverSpidey_bin, 1, 0x10020, fd);
		
		fseek(fd, base_amalga + spideyShade, SEEK_SET);
		fwrite(OG_spideySHADE_bin, 1, 0x8020, fd);
		
		printf("\n%s\n", SP_QUOTES[3]);
	}
	else if(val == 6)
	{
		//Blackpink
		fseek(fd, base_amalga + spideyMain, SEEK_SET);
		fwrite(PinkSpidey_bin, 1, 0x10020, fd);
		
		fseek(fd, base_amalga + spideyShade, SEEK_SET);
		fwrite(OG_spideySHADE_bin, 1, 0x8020, fd);
		
		printf("\n%s\n", SP_QUOTES[5]);
	}
	else if(val == 7)
	{
		//Green
		fseek(fd, base_amalga + spideyMain, SEEK_SET);
		fwrite(GreenSpidey_bin, 1, 0x10020, fd);
		
		fseek(fd, base_amalga + spideyShade, SEEK_SET);
		fwrite(OG_spideySHADE_bin, 1, 0x8020, fd);
		
		printf("\n%s\n", SP_QUOTES[4]);
	}
	else if(val == 8)
	{
		//restore
		fseek(fd, base_amalga + spideyMain, SEEK_SET);
		fwrite(OG_spidey_bin, 1, 0x10020, fd);
		
		u32 pressed;
		printf("Remove suit shading? ");
		do
		{
			VIDEO_WaitVSync();
			get_input(NULL, &pressed);
			if (!pressed)
				continue;
			if (pressed & INPUT_BUTTON_START)
			{
				printf("\nExiting...\n");
				break;
				//goto wait_for_exit;
			}
			if (pressed & INPUT_BUTTON_CANCEL) {
				printf("No.\n");
				fseek(fd, base_amalga + spideyShade, SEEK_SET);
				fwrite(OG_spideySHADE_bin, 1, 0x8020, fd);
				break;
			}
			if (pressed & INPUT_BUTTON_OK)
			{
				printf("Yes.\n");
				u8 dummyData[0x8020] = {0};
				fseek(fd, base_amalga + spideyShade, SEEK_SET);
				fwrite(dummyData, 1, 0x8020, fd);
				
				printf("\n%s\n", SP_QUOTES[0]);
				break;
			}
		} while (1);
		printf("\n");
	}
	else if(val == 9)
	{
		//printf("Under construction!\n\n");
		
		u8* suitBuf = memalign(32, 0x10020);
		u8* shadeBuf = memalign(32, 0x8020);
		bool safeWrite = false;
		FILE* f1 = fopen("disk:/apps/Cool Spidey Outfit/suit.tpl", "rb");
		if(f1 != NULL) {
				unsigned id = 0;
				fseek(f1, 0, SEEK_CUR);
				fread(&id, 1, 4, f1);
				
				if(id == 0x0020AF30) { //It's a TPL
					fseek(f1, 0x44, SEEK_SET);
					fread(&id, 1, 4, f1);
					if(id == 8) {//Correct format
						fseek(f1, 0x48, SEEK_SET);
						fread(&id, 1, 4, f1);
						if(id != 0) {
							fseek(f1, id, SEEK_SET);
							fread(suitBuf, 1, 0x10000, f1);
							
							fseek(f1, 0x1C, SEEK_SET);
							fread(&id, 1, 4, f1);
							
							fseek(f1, id, SEEK_SET);
							fread(&suitBuf[0x10000], 1, 0x20, f1);
							
							fclose(f1);
							printf("\n");
							printf("Obtained suit data from file!\n");
							safeWrite = true;
						}
					}
				}
			} else {
				printf("\n");
				printf("Suit file not found! Keeping original.\n");
			}
			
			fseek(fd, base_amalga + spideyMain, SEEK_SET);
			if(safeWrite)
				fwrite(suitBuf, 1, 0x10020, fd);
			
			safeWrite = false;
			
			FILE* f2 = fopen("disk:/apps/Cool Spidey Outfit/shade.tpl", "rb");
			if(f2 != NULL) {
				unsigned id = 0;
				fseek(f2, 0, SEEK_CUR);
				fread(&id, 1, 4, f2);
				
				if(id == 0x0020AF30) { //It's a TPL
					fseek(f2, 0x44, SEEK_SET);
					fread(&id, 1, 4, f2);
					if(id == 8) {//Correct format
						fseek(f2, 0x48, SEEK_SET);
						fread(&id, 1, 4, f2);
						if(id != 0) {
							fseek(f2, id, SEEK_SET);
							fread(shadeBuf, 1, 0x8000, f2);
							
							fseek(f2, 0x1C, SEEK_SET);
							fread(&id, 1, 4, f2);
							
							fseek(f2, id, SEEK_SET);
							fread(&shadeBuf[0x8000], 1, 0x20, f2);
							
							fclose(f2);
							printf("\n");
							printf("Obtained shade data from file!\n");
							safeWrite = true;
						}
					}
				}
			} else {
				printf("\n");
				printf("Shade file not found! Keeping original.\n");
			}
			
			fseek(fd, base_amalga + spideyShade, SEEK_SET);
			if(safeWrite)
				fwrite(shadeBuf, 1, 0x8020, fd);
		}
}

int main(int argc, char **argv)
{
	GXRModeObj *rmode;

//  USB gecko logging
//	CON_EnableGecko(1, 1);

	VIDEO_Init();

	rmode = VIDEO_GetPreferredMode(NULL);

	initialize(rmode);

	printf("\n\n\n"); // move past screen border

	input_startup();

	printf("%s\n\n\n", "Cool Spidey Outfit  -  v1.0");

	// make sure AHBPROT is disabled
/*	if ((read32(0xCD800038) | read32(0xCD80003C))==0)
	{
		VIDEO_SetBlack(FALSE);
		VIDEO_Flush();
		printf("AHBPROT access is required!\n");
		goto wait_for_exit;
	}*/

	// parse arguments
	bool useSD = false;
	int i;
	for(i=1; i<argc; ++i){
		handleConfigPair(argv[i]);
	}

	// testing
	showInfo = true;

	if(showInfo) {
		VIDEO_SetBlack(FALSE);
		VIDEO_Flush();
	}

	// if no iso path is available
	if(iso_path[0] == 0)
#ifdef HW_RVL
		sprintf(iso_path, "sd:/games/Spider-Man 2 [GK2E52]/game.iso");
#else
		sprintf(iso_path, "sd:/games/Spider-Man 2.iso");
#endif

	// mount device based on iso_path
#ifdef HW_RVL
	if(strncmp(iso_path, "usb", 3) == 0) {
		if (fatMountSimple("disk", &__io_usbstorage)==true)
			printf("Using USB drive.\n");
		else
			printf("Failed to mount USB drive!\n");
	}
	else if(strncmp(iso_path, "sd", 2) == 0) {
		useSD = true;
		if (fatMountSimple("disk", &__io_wiisd)==true)
			printf("Using SD card.\n");
		else
			printf("Failed to mount SD card!\n");
	} else {
		// mount sd as default
		useSD = true;
		if (fatMountSimple("disk", &__io_wiisd)==true)
			printf("Using SD card.\n");
		else
			printf("Failed to mount SD card!\n");
	}
#else
	if(strncmp(iso_path, "sd", 2) == 0) {
		useSD = true;
		if (fatMountSimple("disk", &__io_gcode)==true)
			printf("Mounted SD Card.\n");
		else
			printf("Failed to mount SD card!\n");
	} else if(strncmp(iso_path, "sp", 2) == 0) {
		useSD = true;
		if (fatMountSimple("disk", &__io_gcsd2)==true)
			printf("Using SD Card in SP2.\n");
		else
			printf("Failed to mount SD Card!\n");
	}
#endif
	// modify paths to remove device name
	if(useSD) {
		strcpy(iso_path, &iso_path[3]);
		strcpy(mcd_path, &mcd_path[3]);
	} else {
		strcpy(iso_path, &iso_path[4]);
		strcpy(mcd_path, &mcd_path[4]);
	}
	//printf("Loading %s\n",  iso_path);

	if(strstr(iso_path, ".iso") != NULL) {
		char* image = strstr(iso_path, ".iso");
		if(image != NULL) { // means we have the name of the iso
			u8 i = 0;
			for(i = strlen(iso_path); i > 0; --i) {
				if(iso_path[i] == 0x2F)
					break;
			}
			strcpy(iso_name, &iso_path[i+1]);
			iso_path[i+1] = 0;
		//	printf("Found %s! From dir %s\n", iso_name, iso_path);
		}
	}
	else if(strstr(iso_path, ".gcm") != NULL) {
		char* image = strstr(iso_path, ".gcm");
		if(image != NULL) { // means we have the name of the gcm
			u8 i = 0;
			for(i = strlen(iso_path); i > 0; --i) {
				if(iso_path[i] == 0x2F)
					break;
			}
			strcpy(iso_name, &iso_path[i+1]);
			iso_path[i+1] = 0;
		//	printf("Found %s! From dir %s\n", iso_name, iso_path);
		}
	}

#if 0
/*	playMusic = true;
	sprintf(jingle_path, "sd:/apps/gc_devo/music.ogg");*/

	// play music
	if(playMusic) {
		Sound_Init();
		FILE* bgm_main = NULL;
		int MusicSize = 0;
		bool openSuccess = false;
		// fix path to remove device
		const char *colon = strstr(jingle_path, ":/");
		if(colon != NULL) {
			char tmp[128];
			strcpy(jingle_path, colon+1);          // remove device name
			sprintf(tmp, jingle_path);             // backup path
			sprintf(jingle_path, "disk:/%s", tmp); // mix it all together
		}
		
		bgm_main = fopen(jingle_path, "rb");
		if(bgm_main != NULL) {
			openSuccess = true;
			printf("Found jingle file!\n");
		} else
			printf("Jingle not found: %s\n", jingle_path);
		
		if(!openSuccess) {
			bgm_main = fopen("disk:/apps/gc_devo/music.ogg", "rb");
			if(bgm_main != NULL) {
				openSuccess = true;
				printf("Found default jingle file!\n");
			}
		}
		
		if(openSuccess) {
			fseek(bgm_main, 0, SEEK_END);
			MusicSize = ftell(bgm_main);
			rewind(bgm_main);
			if(MusicSize < 2*1024*1024) {
				mainMusic = memalign(32, MusicSize);
				if(!mainMusic) {
					// failed to allocate space for music
					goto wait_for_exit;
				}
				fread(mainMusic, 1, MusicSize, bgm_main);
			}
			fclose(bgm_main);
			Sound_Play_BGM_Main(mainMusic, MusicSize);
			printf("Playing jingle...\n");
			
			if(timerDone == 0)
				timerDone = gettime();
			
			while(Sound_IsPlaying() == 1) {
				if(diff_sec(timerDone, gettime()) > 60) {
					printf("Too long, skipping...\n");
					break; // exit after a minute
				}
			}
			Sound_Stop();
		}
		usleep(200);
		Sound_Deinit();
	}
#endif

	//easter egg if tpl is found write it to one of the city banners
	FILE* fe = fopen("disk:/apps/Cool Spidey Outfit/tri1.tpl", "rb");
	bool writeE1 = false;
	bool writeE2 = false;
	bool writeE3 = false;
	bool writeE4 = false;
	bool writeE5 = false;
	unsigned e_size = 0;
	u8* e1_buf = memalign(32, tribeca_sz);
	u8* e2_buf = memalign(32, tribeca_sz);
	u8* e3_buf = memalign(32, tribeca_sz);
	u8* e4_buf = memalign(32, title_sz);
	u8* e5_buf = memalign(32, title_sz);
	if(fe != NULL) {
		fseek(fe, 0, SEEK_END);
		e_size = ftell(fe);
		
		//printf("EE1 size: 0x%X\n", e_size);
		
		if((e_size - 0x40) == tribeca_sz) {
			fseek(fe, 0x40, SEEK_SET);
			fread(e1_buf, 1, tribeca_sz, fe);
			
			writeE1 = true;
		}
		fclose(fe);
	}
	
	fe = fopen("disk:/apps/Cool Spidey Outfit/tri2.tpl", "rb");
	if(fe != NULL) {
		fseek(fe, 0, SEEK_END);
		e_size = ftell(fe);
		
		//printf("EE2 size: 0x%X\n", e_size);
		
		if((e_size - 0x40) == tribeca_sz) {
			fseek(fe, 0x40, SEEK_SET);
			fread(e2_buf, 1, tribeca_sz, fe);
			
			writeE2 = true;
		}
		fclose(fe);
	}
	
	fe = fopen("disk:/apps/Cool Spidey Outfit/tri3.tpl", "rb");
	if(fe != NULL) {
		fseek(fe, 0, SEEK_END);
		e_size = ftell(fe);
		
		//printf("EE2 size: 0x%X\n", e_size);
		
		if((e_size - 0x40) == tribeca_sz) {
			fseek(fe, 0x40, SEEK_SET);
			fread(e3_buf, 1, tribeca_sz, fe);
			
			writeE3 = true;
		}
		fclose(fe);
	}
	//TODO: BrawlBox TPLs add 0x60 instead of 0x40, but its CMPR converter is bad
	
	fe = fopen("disk:/apps/Cool Spidey Outfit/title.tpl", "rb");
	if(fe != NULL) {
		fseek(fe, 0, SEEK_END);
		e_size = ftell(fe);
		
		if((e_size - 0x40) == title_sz) {
			fseek(fe, 0x40, SEEK_SET);
			fread(e4_buf, 1, title_sz, fe);
			
			writeE4 = true;
		}
		fclose(fe);
	}
	fe = fopen("disk:/apps/Cool Spidey Outfit/titlemask.tpl", "rb");
	if(fe != NULL) {
		fseek(fe, 0, SEEK_END);
		e_size = ftell(fe);
		
		if((e_size - 0x40) == title_sz) {
			fseek(fe, 0x40, SEEK_SET);
			fread(e5_buf, 1, title_sz, fe);
			
			writeE5 = true;
		}
		fclose(fe);
	}
	//Eastereggs done
	
	//Open ISO
	char full_path[256] = {0};
	sprintf(full_path, "disk:%s%s", iso_path, iso_name);
	
	//printf("Using path: %s\n", full_path);
	
	FILE* fd = fopen(full_path, "r+");
	if(fd == NULL)
		goto wait_for_exit;
	
	bool clean = false;
	bool useNKIT = false;
	int ident = 0;
	fseek(fd, 0, SEEK_CUR);
	fread(&ident, 1, 4, fd);
	
	if(ident == 0x474B3245) {
		//NTSC-U
		fseek(fd, 0x200, SEEK_SET);
		fread(&ident, 1, 4, fd);
		if(ident == 0x4E4B4954) //nkit text
			useNKIT = true;
		else {
			fseek(fd, amalga_addr_clean, SEEK_SET);
			fread(&ident, 1, 4, fd);
			if(ident == 0x0B)
				clean = true;
		}
		
		printf("Game found. %s version.\n", useNKIT ? "NKIT" : "Normal");
		
		unsigned base_amalga = 0;
		if(useNKIT)
			base_amalga = amalga_addr_nkit;
		else if (clean)
			base_amalga = amalga_addr_clean;
		else
			base_amalga = amalga_addr_shrunk;
		
		//Searches the game's fst to find the pak address in the iso
		if(searchFST) {
			fseek(fd, fstPTR, SEEK_SET);
			fread(&ident, 1, 4, fd);
			if(ident > 0) {
				bool foundPAK = false;
				int pos = ident;
				fseek(fd, ident, SEEK_SET);
				//0x5E8 stack to search in
				int i = 0;
				for(i = 8; i<0x5E8; i+=4) {
					fread(&ident, 1, 4, fd);
					if(ident == 0x0C9EB800) {
						fseek(fd, -8, SEEK_CUR); //always (current - 4) to get file offset
						fread(&ident, 1, 4, fd);
						foundPAK = true;
						printf("Found PAK file address.\n");
						break;
					}
					fseek(fd, pos+i, SEEK_SET);
				}
				if(foundPAK)
					base_amalga = ident;
				else
					printf("Search failed, using stock values.\n");
			}
		}
		
		if(forceAddress != 0)
			base_amalga = forceAddress;
		
		//if argument exists
		writeVFilter(fd);
		
		//Select and write suit data to ISO
		writeSuit(fd, base_amalga);
		
		printf("\n");
		
		//easter eggs
		if(writeE1) {
			fseek(fd, base_amalga + goodShotBnr, SEEK_SET);
			fwrite(e1_buf, 1, tribeca_sz, fd);
			printf("EasterEgg 1 applied!\n");
		}
		
		if(writeE2) {
			fseek(fd, base_amalga + designBnr, SEEK_SET);
			fwrite(e2_buf, 1, tribeca_sz, fd);
			printf("EasterEgg 2 applied!\n");
		}
		
		if(writeE3) {
			fseek(fd, base_amalga + artBnr, SEEK_SET);
			fwrite(e3_buf, 1, tribeca_sz, fd);
			printf("EasterEgg 3 applied!\n");
		}
		
		if(writeE4) {
			fseek(fd, base_amalga + titleScreen, SEEK_SET);
			fwrite(e4_buf, 1, title_sz, fd);
			printf("Title screen changed!\n");
		}
		
		if(writeE5) {
			fseek(fd, base_amalga + titleMask, SEEK_SET);
			fwrite(e5_buf, 1, title_sz, fd);
			printf("Title screen mask changed!\n");
		}
	} else if(ident == 0x474B3250) {
		//PAL
		fseek(fd, 0x200, SEEK_SET);
		fread(&ident, 1, 4, fd);
		if(ident == 0x4E4B4954) //nkit text
			useNKIT = true;
		else {
			fseek(fd, amalga_addr_clean, SEEK_SET);
			fread(&ident, 1, 4, fd);
			if(ident == 0x0B)
				clean = true;
		}
		
		printf("Game found. %s version.\n", useNKIT ? "NKIT" : "Normal");
		
		unsigned base_amalga = 0;
		if(useNKIT)
			base_amalga = amalga_addr_nkit_pal;
		else if (clean)
			base_amalga = amalga_addr_clean_pal;
		else
			base_amalga = amalga_addr_shrunk_pal;
		
		if(forceAddress != 0)
			base_amalga = forceAddress;
		
		if(searchFST) {
			fseek(fd, fstPTR, SEEK_SET);
			fread(&ident, 1, 4, fd);
			if(ident > 0) {
				bool foundPAK = false;
				int pos = ident;
				fseek(fd, ident, SEEK_SET);
				//0x5E8 stack to search in
				int i = 0;
				for(i = 8; i<0x5E8; i+=4) {
					fread(&ident, 1, 4, fd);
					if(ident == 0x0C9EB800) {
						fseek(fd, -8, SEEK_CUR); //always (current - 4) to get file offset
						fread(&ident, 1, 4, fd);
						foundPAK = true;
						printf("Found PAK file address.\n");
						break;
					}
					fseek(fd, pos+i, SEEK_SET);
				}
				if(foundPAK)
					base_amalga = ident;
				else
					printf("Search failed, using stock values.\n");
			}
		}
		
		if(forceAddress != 0)
			base_amalga = forceAddress;
		
		//if argument exists
		writeVFilter(fd);
		
		//select and write suit data to ISO
		writeSuit(fd, base_amalga);
		
		printf("\n");
		
		//easter eggs
		if(writeE1) {
			fseek(fd, base_amalga + goodShotBnr, SEEK_SET);
			fwrite(e1_buf, 1, tribeca_sz, fd);
			printf("EasterEgg 1 applied!\n");
		}
		
		if(writeE2) {
			fseek(fd, base_amalga + designBnr, SEEK_SET);
			fwrite(e2_buf, 1, tribeca_sz, fd);
			printf("EasterEgg 2 applied!\n");
		}
		
		if(writeE3) {
			fseek(fd, base_amalga + artBnr, SEEK_SET);
			fwrite(e3_buf, 1, tribeca_sz, fd);
			printf("EasterEgg 3 applied!\n");
		}
		
		if(writeE4) {
			fseek(fd, base_amalga + titleScreen, SEEK_SET);
			fwrite(e4_buf, 1, title_sz, fd);
			printf("Title screen changed!\n");
		}
		
		if(writeE5) {
			fseek(fd, base_amalga + titleMask, SEEK_SET);
			fwrite(e5_buf, 1, title_sz, fd);
			printf("Title screen mask changed!\n");
		}
	} else {
		//nothing
		printf("Data is not Spider-Man 2!\n\n");
		fclose(fd);
		goto wait_for_exit;
	}
	
	fclose(fd);

	// unmount disk to be sure any changes are flushed
	fatUnmount("disk");

	printf("\n\n");

wait_for_exit:
	printf("Press START to exit.\n");
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
#if 1
	while(1) {
		u32 pressed;
		get_input(NULL, &pressed);
		if (pressed & INPUT_BUTTON_START) break;
		VIDEO_WaitVSync();
	}
#endif
	// Needed on gamecube when loading through swiss
	VIDEO_SetBlack(true);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	return 0;
}
