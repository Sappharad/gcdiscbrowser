/* Low-level DVD functions, provided by Parasyte */

#ifndef _DVD_H_
#define _DVD_H_

//#include "types.h"


#define DVD_STATUS1	*(vu32*)(0xCC006000)
#define DVD_STATUS2	*(vu32*)(0xCC006004)
#define DVD_COMMAND	*(vu32*)(0xCC006008)
#define DVD_OFFSET	*(vu32*)(0xCC00600C)
#define DVD_SRCLEN	*(vu32*)(0xCC006010)
#define DVD_DSTBUF	*(vu32*)(0xCC006014)
#define DVD_DSTLEN	*(vu32*)(0xCC006018)
#define DVD_ACTION	*(vu32*)(0xCC00601C)
#define DVD_ERROR	*(vu32*)(0xCC006020)
#define DVD_CONFIG	*(vu32*)(0xCC006024)


int DVDInit();
u8 *DVDVersionString();
u8 DVDVersion();
u8 *DVDRegionString();
int DVDRegionJ();
int DVDRegionU();
int DVDRegionP();
int DVDAudioStatus(u8 status, u8 size);
u32 DVDRequestError();
int DVDLidOpen();
int DVDLidChange();
void DVDInitDrive();
void DVDResetDrive();
void DVDStopDrive();
u32 DVDGetDiscID();
u32 DVDReadRaw(void *dst, u32 src, u32 len);
void DVDSeekRaw(u32 offset);
void DVDStreamRaw(u32 src, u32 len, u8 subcmd);

#endif //_DVD_H_
