#include <ogcsys.h>
#include <asndlib.h>

//#include "oggplayer.h"
#include "sound.h"

/* Externs */
//extern u8  bgMusic[];
//extern u32 bgMusic_Len;

void Sound_Init(void)
{
	/* Init ASND */
	ASND_Init();
}

void Sound_Deinit(void)
{
	/* Init ASND */
	ASND_End();
} 

s32 Sound_Play_BGM_Main(u8* data, int size)
{
	return 0;//PlayOgg(data, size, 0, OGG_ONE_TIME);
}

void Sound_Stop(void)
{
	/* Stop background music */
	return;//if (Sound_IsPlaying())
		//StopOgg();
}

s32 Sound_IsPlaying(void)
{
	/* Check if background music is playing */
	return 0;//(StatusOgg() == OGG_STATUS_RUNNING);
}
