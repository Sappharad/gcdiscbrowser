#include <GC.h>

/** Unthreaded Gamecube ADX Library version 1.2
* ADX decoder based on adx2wav by bero.
* How to use: To start playing some audio, pass the ADX file, already in memory, to the startadx() function.
* Then call update_adx() whenever AUDIO_DMA_LEFT is about to run out. Make sure you check it frequently.
*
* Version 1.0 - Initial version,
* Version 1.1 - Rearranged some things so playback is a bit smoother.
* Version 1.2 - Resampling code added so all common bitrates are supported.
*/

#define AUDIO_DMA_STARTH    *(volatile u16*)(0xCC005030)
#define AUDIO_DMA_STARTL    *(volatile u16*)(0xCC005032)
#define AUDIO_DMA_LENGTH    *(volatile u16*)(0xCC005036)
#define AUDIO_STREAM_STATUS *(volatile u32*)(0xCC006C00)
#define AUDIO_DMA_LEFT      *(volatile u16*)(0xCC00503a)
#define LoadSample(addr, len) AUDIO_DMA_STARTH = addr >> 16;       \
                              AUDIO_DMA_STARTL = addr & 0xffff;    \
                              AUDIO_DMA_LENGTH = (AUDIO_DMA_LENGTH & 0x8000) | (len / 32)
/* Set the high bit of the length register */
#define StartSample()  AUDIO_DMA_LENGTH |= 0x8000
/* Clear the high bit of the length register */
#define StopSample()   AUDIO_DMA_LENGTH &= 0x7fff
/* Set the 6th bit of the stream status register */
#define SetFreq32KHz() AUDIO_STREAM_STATUS |= 0x40
/* Clear the 6th bit of the stream status register */
#define SetFreq48KHz() AUDIO_STREAM_STATUS &= 0xffffffbf

long read_long(unsigned char *p)
{
	return (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
}
int read_word(unsigned char *p)
{
	return (p[0]<<8)|p[1];
}
typedef struct {
	int s1,s2;
} PREV;

unsigned char *adxdata_myfile;
unsigned long adxdata_filepos;
unsigned long adxdata_buffersize;
int adxdata_channel,adxdata_freq,adxdata_size,adxdata_wsize;
PREV adxdata_prev[2];

#define	BASEVOL	0x4000

int convert(short *out,unsigned char *in,PREV *prev)
{
	int scale = ((in[0]<<8)|(in[1]));
	int i;
	int s0,s1,s2,d;

	in+=2;
	s1 = prev->s1;
	s2 = prev->s2;
	for(i=0;i<16;i++) {
		d = in[i]>>4;
		if (d&8) d-=16;
		s0 = (BASEVOL*d*scale + 0x7298*s1 - 0x3350*s2)>>14;
		if (s0>32767) s0=32767;
		else if (s0<-32768) s0=-32768;
		*out++=s0;
		s2 = s1;
		s1 = s0;

		d = in[i]&15;
		if (d&8) d-=16;
		s0 = (BASEVOL*d*scale + 0x7298*s1 - 0x3350*s2)>>14;
		if (s0>32767) s0=32767;
		else if (s0<-32768) s0=-32768;
		*out++=s0;
		s2 = s1;
		s1 = s0;
	}
	prev->s1 = s1;
	prev->s2 = s2;

}

/** Start playing the ADX file, already in memory, that is passed into this function
*@param inmusic The music data
*@return How many chunks of data are left in the ADX file to decode, -1 if not an ADX file
*/
int startadx(unsigned char *inmusic)
{
	int loopval;
	int bufferfill;
	unsigned char buf[18*2];
	short outbuf[64*2048];
	int offset;
	
	u16 *mpointer;

	//Setup global stuff needed to play in the background
	adxdata_myfile = inmusic;
	adxdata_filepos=0;

	GC_Memory_memcpy(buf,adxdata_myfile+adxdata_filepos,16);
	adxdata_filepos+=16;

	adxdata_channel = buf[7];
	adxdata_freq = read_long(buf+8);
	adxdata_size = read_long(buf+12);

	offset = read_word(buf+2)-2;
	adxdata_filepos = offset;
	GC_Memory_memcpy(buf+1,adxdata_myfile+adxdata_filepos,6);
	adxdata_filepos+=6;

	if (buf[0]!=0x80 || GC_Memory_memcmp(buf+1,"(c)CRI",6)) {
		GC_Debug_RStringT ("Not ADX!", 0xFF80FF80, 50, 100);
		return -1;
	}

	adxdata_prev[0].s1 = 0;
	adxdata_prev[0].s2 = 0;
	adxdata_prev[1].s1 = 0;
	adxdata_prev[1].s2 = 0;

	//Setup frequency
	if(adxdata_freq == 32000)
		SetFreq32KHz();
	else
		SetFreq48KHz();

	if (adxdata_channel==1){
		adxdata_buffersize=0;
		for (bufferfill=0; bufferfill<2048; bufferfill++){
			GC_Memory_memcpy(buf,adxdata_myfile+adxdata_filepos,18);
			adxdata_filepos+=(18);

			convert(outbuf+(bufferfill*32),buf,adxdata_prev);
			if (adxdata_size>32) adxdata_wsize=32; else adxdata_wsize = adxdata_size;
			adxdata_size-=adxdata_wsize;

			adxdata_buffersize+=(adxdata_wsize*2);

			if(adxdata_size == 0)
				break;
		}

		mpointer = (u16*)0x81710000;
		//Merge left and right sound channels into a stereo sample
		for(loopval=0; loopval<adxdata_buffersize;loopval++){
			 *mpointer++ = outbuf[loopval];
			 *mpointer++ = outbuf[loopval];
		}
		
	}
	else if (adxdata_channel==2){
		short tmpbuf[32*2];
		int i;

		adxdata_buffersize=0;

		for (bufferfill=0; bufferfill<2048; bufferfill++){
			GC_Memory_memcpy(buf,adxdata_myfile+adxdata_filepos,18*2);
			adxdata_filepos+=(18*2);
			convert(tmpbuf,buf,adxdata_prev);
			convert(tmpbuf+32,buf+18,adxdata_prev+1);
			for(i=0;i<32;i++) {
				outbuf[(i*2)+(bufferfill*64)]   = tmpbuf[i];
				outbuf[(i*2+1)+(bufferfill*64)] = tmpbuf[i+32];
			}
			if (adxdata_size>32) adxdata_wsize=32; else adxdata_wsize = adxdata_size;
			adxdata_size-=adxdata_wsize;

			adxdata_buffersize+=(adxdata_wsize*2);

			if(adxdata_size == 0)
				break;
		}

		mpointer = (u16*)0x81710000;
		for(loopval=0; loopval<adxdata_buffersize;loopval++){
			 *mpointer++ = outbuf[loopval];
		}
	}

	//Common Code for upsampling and playback
	if((adxdata_freq != 32000) && (adxdata_freq < 48000)){
		unsigned long outsize = (adxdata_buffersize*3000)/(adxdata_freq>>4);
		outsize &= 0xFFFFFFFE; //Make sure it's an even number
		short interpo[outsize]; //(64*2048)*48000/44100
		unsigned long incr = (adxdata_freq<<12)/3000; //(44100*(1<<16))/48000;

		//Special thanks to WinterMute for the working resample code, and tmbinc for inital help.

		mpointer = (short *)0x81710000; //Back here again
		short *interp = interpo;
		short sample0, sample1, newsample;
		unsigned long pointer = 0;
		for (loopval = 0; loopval<outsize; loopval+=2) {
			int offset = (pointer>>16) * 2;
			int fraction = pointer & 0xffff;

			sample0 = mpointer[offset];
			sample1 = mpointer[offset + 2];

			newsample = sample0 + (((sample1 - sample0) * fraction) >> 16); 

			*(interp++) = newsample;

			sample0 = mpointer[offset + 1];
			sample1 = mpointer[offset + 3];

			newsample = sample0 + (((sample1 - sample0) * fraction) >> 16); 

			*(interp++) = newsample;

			pointer += incr;
		}

		GC_Memory_DCFlushRange ((void*)interpo, outsize*2);
		LoadSample((u32)interpo,outsize*2);
		StartSample();
	}
	else{
		//No upsampling required.
		GC_Memory_DCFlushRange ((void*)0x81710000, adxdata_buffersize*2);
		LoadSample((u32)0x81710000,adxdata_buffersize*2);
		StartSample();
	}

	return 0;
}

/** Continue playing an already started ADX file.
*@return How many chunks of data are left in the ADX file to decode, 1 if the file is done.
*/
int update_adx()
{
	int loopval;
	int bufferfill;
	unsigned char buf[18*2];
	short outbuf[64*2048];
	int offset;
	u16 *mpointer;

	if (adxdata_size == 0)
		return 1;

	if (adxdata_channel==1){
		while(AUDIO_DMA_LEFT > 4);
		
		adxdata_buffersize=0;
		for (bufferfill=0; bufferfill<2048; bufferfill++){
			GC_Memory_memcpy(buf,adxdata_myfile+adxdata_filepos,18);
			adxdata_filepos+=(18);

			convert(outbuf+(bufferfill*32),buf,adxdata_prev);
			if (adxdata_size>32) adxdata_wsize=32; else adxdata_wsize = adxdata_size;
			adxdata_size-=adxdata_wsize;

			adxdata_buffersize+=(adxdata_wsize*2);

			if(adxdata_size == 0)
				break;
		}

		mpointer = (u16*)0x81710000;
		//Merge left and right sound channels into a stereo sample
		for(loopval=0; loopval<adxdata_buffersize;loopval++){
			 *mpointer++ = outbuf[loopval];
			 *mpointer++ = outbuf[loopval];
		}
	
		
	}
	else if (adxdata_channel==2){
		short tmpbuf[32*2];
		int i;

		while(AUDIO_DMA_LEFT > 4);
		
		adxdata_buffersize=0;
		for (bufferfill=0; bufferfill<2048; bufferfill++){
			GC_Memory_memcpy(buf,adxdata_myfile+adxdata_filepos,18*2);
			adxdata_filepos+=(18*2);
			convert(tmpbuf,buf,adxdata_prev);
			convert(tmpbuf+32,buf+18,adxdata_prev+1);
			for(i=0;i<32;i++) {
				outbuf[(i*2)+(bufferfill*64)]   = tmpbuf[i];
				outbuf[(i*2+1)+(bufferfill*64)] = tmpbuf[i+32];
			}
			if (adxdata_size>32) adxdata_wsize=32; else adxdata_wsize = adxdata_size;
			adxdata_size-=adxdata_wsize;

			adxdata_buffersize+=(adxdata_wsize*2);

			if(adxdata_size == 0)
				break;
		}

		mpointer = (u16*)0x81710000;
		for(loopval=0; loopval<adxdata_buffersize;loopval++){
			 *mpointer++ = outbuf[loopval];
		}
	}

	//Common Code for upsampling and playback
	if((adxdata_freq != 32000) && (adxdata_freq < 48000)){
		unsigned long outsize = (adxdata_buffersize*3000)/(adxdata_freq>>4);
		outsize &= 0xFFFFFFFE; //Make sure it's an even number
		short interpo[outsize]; //(64*2048)*48000/44100
		unsigned long incr = (adxdata_freq<<12)/3000; //(44100*(1<<16))/48000;

		//Special thanks to WinterMute for the working resample code, and tmbinc for inital help.

		mpointer = (short *)0x81710000; //Back here again
		short *interp = interpo;
		short sample0, sample1, newsample;
		unsigned long pointer = 0;
		for (loopval = 0; loopval<outsize; loopval+=2) {
			int offset = (pointer>>16) * 2;
			int fraction = pointer & 0xffff;

			sample0 = mpointer[offset];
			sample1 = mpointer[offset + 2];

			newsample = sample0 + (((sample1 - sample0) * fraction) >> 16); 

			*(interp++) = newsample;

			sample0 = mpointer[offset + 1];
			sample1 = mpointer[offset + 3];

			newsample = sample0 + (((sample1 - sample0) * fraction) >> 16); 

			*(interp++) = newsample;

			pointer += incr;
		}

		GC_Memory_DCFlushRange ((void*)interpo, outsize*2);
		LoadSample((u32)interpo,outsize*2);
		StartSample();
	}
	else{
		//No upsampling required.
		GC_Memory_DCFlushRange ((void*)0x81710000, adxdata_buffersize*2);
		LoadSample((u32)0x81710000,adxdata_buffersize*2);
		StartSample();
	}


	return 0;
}

