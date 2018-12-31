/* Low-level DVD functions, provided by Parasyte */

#include "dvd.h"


static char *region_string[4] = { "NTSC-Japan","NTSC-U.S.A","PAL-Europe","Region Unknown" };
static char *version_string[2] = { "Version 1.X","Version Unknown" };

int DVDInit() {
	if (DVDLidOpen()) return 1;
	DVDInitDrive();
	DVDResetDrive();
	DVDGetDiscID();
	if (*(u8*)(0x80000008)) {
		if (DVDAudioStatus(1, (*(u8*)(0x80000009) ? 0 : 0x0A))) return 1;
	}
	else if (DVDAudioStatus(0,0)) return 1;
	return 0;
}

u8 *DVDRegionString() {
	if(DVDRegionJ()) return region_string[0];
	if(DVDRegionU()) return region_string[1];
	if(DVDRegionP()) return region_string[2];
	return region_string[3];
}

u8 DVDVersion() {
	return *(u8*)(0x80000007);
}

u8 *DVDVersionString() {
	if (DVDVersion() < 10) {
		version_string[0][10] = (DVDVersion()+0x30);
		return version_string[0];
	}
	return version_string[1];
}

int DVDRegionJ() {
	if (*(u8*)(0x80000003) == 'J') return 1;
	return 0;
}

int DVDRegionP() {
	if (*(u8*)(0x80000003) == 'P') return 1;
	return 0;
}

int DVDRegionU() {
	if (*(u8*)(0x80000003) == 'E') return 1;
	return 0;
}
/*
int DVDAudioDisable() {
	u32 i;

	//for (i = 0; i < 0x01000000; i++);
	DVD_COMMAND = 0xE4000000;
	DVD_ACTION = 1;

	for (i = 0; i < 0x100000; i++);
	while (1) {
		if (DVD_STATUS1 & 4) return 1;
		if (!DVD_DSTLEN) return 0;
	}
}

int DVDAudioEnable(u8 size) {
	u32 i;

	//for (i = 0; i < 0x01000000; i++);
	DVD_COMMAND = (0xE4010000|size);
	DVD_ACTION = 1;

	for (i = 0; i < 0x100000; i++);
	while (1) {
		if (DVD_STATUS1 & 4) return 1;
		if (!DVD_DSTLEN) return 0;
	}
}
*/
int DVDAudioStatus(u8 status, u8 size) {
	u32 i;

	//for (i = 0; i < 0x01000000; i++);
	DVD_STATUS2 = DVD_STATUS2;
	DVD_COMMAND = (0xE4000000|((!!status) << 16)|size);
	DVD_ACTION = 1;

	//wait
	for (i = 0; i < 0x100000; i++);
	while (DVD_ACTION & 1) {
		if (DVD_STATUS1 & 4) return 1;
	}
	return 0;
}

u32 DVDRequestError() {
	u32 i;

	DVD_COMMAND = 0xE0000000;
	DVD_ACTION = 1;
	for (i = 0; i < 0x10000; i++);
	return DVD_ERROR;
}

int DVDLidOpen() {
	int ret = (DVD_STATUS2 & 1);

	DVD_STATUS2 |= 0x04;
	return ret;
}

int DVDLidChange() {
	int ret = (!!(DVD_STATUS2 & 0x04));

	DVD_STATUS2 |= 0x04;
	return ret;
}

void DVDInitDrive() {
	DVD_STATUS1 = 0x3E; //0x2A;
	DVD_STATUS2 = 0x00;
}

void DVDResetDrive() {
	u32 i,tmp;

	DVDInitDrive();

	tmp = *(u32*)0xCC003024;
	*(u32*)0xCC003024 = (tmp&0xFFFFFFFB)|1;
	for (i = 0; i < 0x100000; i++);
	*(u32*)0xCC003024 = tmp|5;
	for (i = 0; i < 0x1000; i++);
}

void DVDStopDrive() {
	DVD_COMMAND = 0xE3000000;
	DVD_ACTION = 1;
}

u32 DVDGetDiscID() {
	DVD_COMMAND = 0xA8000040;
	DVD_OFFSET = 0;
	DVD_SRCLEN = 0x20;
	DVD_DSTBUF = 0x80000000;
	DVD_DSTLEN = 0x20;
	DVD_ACTION = 3;

	while (DVD_ACTION & 1) {
		if (DVD_STATUS1 & 4) return (DVD_DSTLEN?DVD_DSTLEN:1);
	}

	GC_Memory_DCFlushRange((void*)0x80000000, 0x20);

	return DVD_DSTLEN;
}

u32 DVDReadRaw(void *dst, u32 src, u32 len) {
	dst = (void*)(((u32)dst)&~0x1F);
	len = ((len+0x1F)&~0x1F);
	DVD_COMMAND = 0xA8000000;
	DVD_OFFSET = (src >> 2);
	DVD_SRCLEN = len;
	DVD_DSTBUF = (u32)(dst);
	DVD_DSTLEN = len;
	DVD_ACTION = 3;

	while (DVD_ACTION & 1) {
		if (DVD_STATUS1 & 4) return (DVD_DSTLEN?DVD_DSTLEN:1);
	}

	if (((u32)(dst) & 0xC0000000) == 0x80000000) {
		GC_Memory_DCFlushRange(dst, len);
		GC_Memory_ICInvalidateRange(dst, len);
	}

	return DVD_DSTLEN;
}

void DVDSeekRaw(u32 offset) {
	u32 i;

	DVD_COMMAND = 0xAB000000;
	DVD_OFFSET = (offset >> 2);
	DVD_ACTION = 1;

	//wait
	for (i = 0; i < 0x100000; i++);
}

void DVDStreamRaw(u32 src, u32 len, u8 subcmd) {
	DVD_COMMAND = (0xE1000000 | (subcmd << 16));
	DVD_OFFSET = (src >> 2);
	DVD_SRCLEN = len;
	DVD_ACTION = 1;
}
