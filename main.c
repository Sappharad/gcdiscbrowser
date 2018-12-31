/**
 Gamecube Disc Browser
 Created 2004, last updated 2006
 */
#include <GC.h>
#include "font.c"
#include "fontmap.c"
#include "ul.raw.c"
#include "l.raw.c"
#include "bl.raw.c"
#include "b.raw.c"
#include "br.raw.c"
#include "r.raw.c"
#include "ur.raw.c"
#include "u.raw.c"
#include "dvd.c"
#include "adxlib.c"


#ifndef NULL 
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void*)0)
#endif
#endif


//Global Stuff
unsigned long *FrameBuffer;
char gamename[38];
char folderpath[50];
u32 fstfiles;

typedef struct {
  unsigned long filestart;
  unsigned long filelength;
  char filename[32];
  char visible;
} FST_entry;

typedef struct _dirKeeper{
	int position; //Currently selected file, so draw blue behind it!
	int viewtop; //Current file that's at the top of the screen
	int viewbottom; //Current file at the bottom of the screen.
	int windowstart, windowend; //The current range of displayed files
	char folderpath[50]; //The currently displayed path at the top of the screen
	struct _dirKeeper* previous;
} dirKeeper; //Keeps track of the previous folder you're in, for easy going back.

//Draw a character to the screen. Return's the character ID drawn
int drawchar(char letter, int x, int y);
//Draw a string
void draw_string(char *thestring, int x, int y);
//Find the width of string to be drawn
int get_string_width(char *thestring);
//Draw an image
void drawimage(const unsigned long *sprite, int x, int y, int width, int height);
//Draw a dialog box
void drawbox(int x, int y, int width, int height);
//Load FST into ram - return 0 if successful, 1 if error, -1 if blocked.
int loadfst();
//The actual browser function
void browse_disc();
//Determines which files in the FST are visible
void findvisible(FST_entry myfst[], int windowstart, int windowend);

//This line makes good stuff happen... Which is why I commented it out.
//asm(".align 15");

int main (void)
{
	int temp;
	int boxwidth = 300;
	u32 fstinram;

	// Initialize the system.
	GC_System_Init ();

	// Video and framebuffer initialization.
	FrameBuffer = (unsigned long*)0xC0600000;
	// Set NTSC mode.
	GC_Video_Init (VI_NTSC_640x480);
	GC_Debug_Init (FrameBuffer, VI_NTSC_640x480);
	// Set the framebuffer.
	GC_Video_SetFrameBuffer (FrameBuffer, VI_FRAMEBUFFER_BOTH);
	// Clear framebuffer to black.
	GC_Video_ClearFrameBuffer (FrameBuffer, 0x00800080);

	// Wait two frames to allow TV to sync.
	GC_Video_WaitForVBlank ();
	GC_Video_WaitForVBlank ();

	// Enable interrupts.
	//GC_Interrupts_Enable ();

	// Setup the Custom font
	init_goudy_hundred();

	draw_string("Gamecube DVD Disc Browser",(320 - get_string_width("Gamecube DVD Disc Browser")/2),32);
	draw_string("By Sappharad",(320 - get_string_width("By Sappharad")/2),52);
	draw_string("Disc Browser version 2.1",(320 - get_string_width("Disc Browser version 2.1")/2),440);

	for(temp=20; temp<=100; temp+=4){
		drawbox((320-boxwidth/2),(240-temp/2),boxwidth,temp);
		GC_Video_WaitForVBlank ();
	}
	draw_string("Open the disc drive",(320 - get_string_width("Open the disc drive")/2),232);

	//Get ready to use the drive.
	DVDStopDrive(); //This breaks something. Dont use it

	//Wait for drive to open
	while(!DVDLidOpen());

	while (1){
		if(DVDLidOpen()){
			if(boxwidth>0){
				while (boxwidth < 400){
					drawbox((320-boxwidth/2),190,boxwidth,100);
					draw_string("Open the disc drive",(320 - get_string_width("Open the disc drive")/2),232);
					boxwidth += 5;
					GC_Video_WaitForVBlank ();
				}
			}
			drawbox((320-boxwidth/2),190,boxwidth,100);
			draw_string("Insert GC disc and close the drive.",(320 - get_string_width("Insert GC disc and close the drive.")/2),232);
			GC_Video_WaitForVBlank ();
		}
		else{
			if(boxwidth > 0){
				//They closed the lid!
				drawbox((320-boxwidth/2),190,boxwidth,100);
				draw_string("Reading Disc...",(320 - get_string_width("Reading Disc...")/2),232);
				GC_Video_WaitForVBlank ();

				temp = loadfst(); //Load the FST into ram

				browse_disc();
			}
		}
	}

	return 0;
}

int drawchar(char letter, int x, int y){
	int findchar;
	int loopx, loopy;
	u8 temphold = 0;
	unsigned long tempval;

	for(findchar = 0; findchar < 86; findchar++){
		if(goudyhundred[findchar].letter == letter){
			break;
		}
	}
	if (findchar == 86)
		findchar = 85;

	x/=2;

	for (loopx = 0; loopx < (goudyhundred[findchar].width/2); loopx++){
		for(loopy = 0; loopy < goudyhundred[findchar].height; loopy++){
			if (loopx+x < 320){
				temphold = font[goudyhundred[findchar].y+loopy][(goudyhundred[findchar].x/2)+loopx];
				if (temphold != 0){
					//Actually need to draw something here
					tempval = (FrameBuffer[(320*(loopy+y))+loopx+x]);
					tempval = tempval & 0x00FF00FF;
					tempval += (((temphold & 0xF0) | (temphold >> 4)) << 24);
					tempval += (((temphold & 0xF) | ((temphold & 0xF) << 4)) << 8);
					FrameBuffer[(320*(loopy+y))+loopx+x] = tempval;
				}	
			}
		}
	}

	return findchar;
}

void draw_string(char *thestring, int x, int y){
	int currentx = x;
	char currentletter = 85;
	int temp = 0;

	currentletter = thestring[temp];
	while (currentletter != 0){
		currentx += goudyhundred[drawchar(currentletter, currentx, y)].width;
		temp++;
		currentletter = thestring[temp];
	}
}

int get_string_width(char *thestring){
	int currentw = 0;
	char currentletter = 85;
	int temp1=0, temp2=0;
	int lettervalue = 83; //If the loop fails, print a space
	int counter = 0;

	currentletter = thestring[temp2];

	while (currentletter != 0){
		lettervalue = 85;
		for (temp1=0; temp1 <86; temp1++){
			if(goudyhundred[temp1].letter==currentletter){
				lettervalue = temp1;
				break;
			}
		}
		currentw += goudyhundred[lettervalue].width;

		temp2++;
		currentletter = thestring[temp2];
	}

	return currentw;
}

void append(char *thestring, char *addition, int maxlen){
	int current=0;
	int seek=0;

	while(thestring[current] != 0){
		current++;
	}
	//current is now a null char, where we append from
	if(current >= maxlen)
		return;
	while(addition[seek] != 0){
		thestring[current+seek] = addition[seek];
		seek++;
		if(current+seek == maxlen)
			break;
	}
	thestring[current+seek] = 0;
}

int isadx(char *filename){
	int current=0;
	
	while(filename[current] != 0){
		if(filename[current] == 'a' || filename[current] == 'A'){
			if(filename[current+1] == 'd' || filename[current+1] == 'D'){
				if(filename[current+2] == 'x' || filename[current+2] == 'X')
					return 1;
			}
		}
		current++;
	}
	return 0;
}

volatile void hextostring(unsigned long value, char* string){
   char *e = string;
   char *b = string;
 
   if (value == 0){
       string[0] = '0';
       string[1] = 0;
       return;
   }
  
   while (value != 0){
	int mod = value % 16;
	value = value / 16;
	if(mod < 10)
		*e++ = '0' + mod;
	else
		*e++ = 'A' + (mod-10);
   }
 
   *e-- = 0;
 
   //Reverse String
 
   while (e > b){
       char temp = *e;
       *e = *b;
       *b = temp;
       ++b;
       --e;
     }
}

void drawimage(const unsigned long *sprite, int x, int y, int width, int height){
	int cpx, cpy;

	x/=2;

	for(cpx=0; cpx<(width/2); cpx++){
		for(cpy=0; cpy<height; cpy++){
			FrameBuffer[(320*(cpy+y))+cpx+x] = sprite[(cpy*(width/2))+cpx];
		}
	}
}

void drawbox(int x, int y, int width, int height){
	int slide;
	int spillx, spilly;
	int speedfix;

	//Draw Corners
	drawimage(*ul_Bitmap,x,y,20,20);
	drawimage(*ur_Bitmap,x+width-20,y,20,20);
	drawimage(*bl_Bitmap,x,y+height-20,20,20);
	drawimage(*br_Bitmap,x+width-20,y+height-20,20,20);
	//Fill top/bottom
	for(slide=20; slide<(width-20); slide+=2){
		drawimage(*u_Bitmap,x+slide,y,2,20);
		drawimage(*b_Bitmap,x+slide,y+height-20,2,20);
	}
	//Fill left
	for(slide=20; slide<(height-20); slide++){
		drawimage(*l_Bitmap,x,y+slide,20,1);
		drawimage(*r_Bitmap,x+width-20,y+slide,20,1);
	}
	//Fill in box with 0x0BB20B77
	speedfix = (x+width-20)/2;
	for(spillx=(x/2)+10; spillx<speedfix;spillx++){
		for(spilly=y+20;spilly<y+height-20;spilly++){
			FrameBuffer[(320*spilly)+spillx] = 0x0BB20B77;
		}
	}
}

void fillblack(int x, int y, int width, int height){
	int slide;
	int spillx, spilly;
	int speedfix;

	//Fill in box with black
	speedfix = (x+width)/2;
	for(spillx=(x/2); spillx<speedfix;spillx++){
		for(spilly=y;spilly<y+height;spilly++){
			FrameBuffer[(320*spilly)+spillx] = 0x00800080;
		}
	}
}

int loadfst(){
	int temp2;
	u32 blah;
	u32 fstaddy, fstlen, fstinram;
	
	blah = DVDInit();
	if (blah != 0)
	{
		//Unable to read disc
		return 1;
	}

	blah = DVDReadRaw((void*)0xC1000000, 0x00, 0x500);
	GC_Memory_memcpy(gamename, (u8*)0xC1000020, 32);
	GC_Memory_memcpy(gamename+32, (u8*)0xC1000040, 5);

	fstaddy = *(u32*)0xC1000424;
	fstlen = *(u32*)0xC1000428;
	//Get the Location of the FST when in RAM
	fstinram = *(u32*)0xC1000430;
	fstinram -= 0x7EC00000;
	//This is the FST in ram offset from 0.

	while (fstlen % 32 != 0){
		fstlen += 1;
	}

	if (blah != 0)
	{
		return 1;
	}
	
	for (blah = 0; blah < 0xFFFF0000; blah++); //Small wait
	blah = DVDReadRaw((void*)0xC1000000, fstaddy, fstlen);
	fstfiles = *(u32*)0xC1000008;
	fstfiles -= 1;
	
	if (blah != 0)
	{
		return 1;
	}

	return 0;
}

void browse_disc(){
	FST_entry thefst[fstfiles];
	int temp, mychar;
	char letter;
	int position; //Currently selected file, so draw blue behind it!
	int viewtop=0; //Current file that's at the top of the screen
	int viewbottom=18; //Current file at the bottom of the screen.
	int windowstart, windowend; //The current range of displayed files
	int currentfile; //This is the current file that's being drawn to screen
	dirKeeper *backtrack = NULL;
	GC_Pad gc_Pad;

	//Clear screen
	GC_Video_ClearFrameBuffer (FrameBuffer, 0x00800080);
	draw_string("Gamecube DVD Disc Browser",(320 - get_string_width("Gamecube DVD Disc Browser")/2),32);
	draw_string(gamename,(320 - get_string_width(gamename)/2),52);
	GC_Video_WaitForVBlank();

	//Fill up the FST!
	for(temp=0; temp<fstfiles; temp++){
		thefst[temp].filestart = *(u32*)(0xC1000000+16+(temp*12));
		thefst[temp].filelength = *(u32*)(0xC1000000+16+(temp*12)+4);
	}
	position=12+(fstfiles*12);
	mychar=0;
	temp=0;
	while (temp < fstfiles){
		letter = *(char*)(0xC1000000+position);
		position++;
		thefst[temp].filename[mychar] = letter;
		mychar++;
		if(letter == 0){
			temp++;
			mychar=0;
		}
	}

	position = 0; //Currently selected file
	windowstart=0;
	windowend = fstfiles;
	findvisible(thefst, windowstart, windowend);
	folderpath[0] = '/';	folderpath[1] = 0;

	//Main file selection loop
	while(!DVDLidOpen()){
		//Calculate viewbottom
		temp=0;
		viewbottom = viewtop;
		while(temp<16){
			if(thefst[viewbottom].visible==1)
				temp++;
			viewbottom++;
			if(viewbottom == windowend)
				break;
		}

		GC_PAD_Read (&gc_Pad, GC_PAD1); //Read the pad
		if((gc_Pad.button & PAD_DOWN) || (gc_Pad.y > 50)){
			if((position < (windowend-1)) && (viewbottom < windowend))
				position++;
			else if((viewbottom == windowend) && (position < (viewbottom-1)))
				position++;
			while((position < windowend) && (thefst[position].visible == 0))
				position++;

			while(thefst[position].visible == 0){
				position--; //Fixes the problem if the last item is a folder...
			}

			if((position >= viewbottom) && (viewbottom != windowend)){
				viewtop++;
				while(thefst[viewtop].visible==0)
					viewtop++;
			}
		}
		if((gc_Pad.button & PAD_UP) || (gc_Pad.y < -50)){
			position--;
			if(position < windowstart)
				position = windowstart;
			while(thefst[position].visible == 0)
				position--;

			if(position < viewtop)
				viewtop = position;
		}
		if(gc_Pad.button & PAD_L){
			position = windowstart;
			viewtop = position;
		}
		else if(gc_Pad.button & PAD_R){
			position = windowend-1;

			while((position > 0) && (thefst[position].visible == 0))
				position--; //Make sure we've got something visible. :-\

			int howmany=1,theposn;
			theposn = position;
			while(howmany<16){
				if(theposn == windowstart)
					break;
				if(thefst[theposn].visible == 1)
					howmany++;
				theposn--;
			}

			viewtop = theposn;
		}
		if(gc_Pad.button & PAD_A){
			if(thefst[position].filestart < 0x1000){ //Folder
				//First, back up the information we'll need to leave the folder
				dirKeeper *tmpnwdir = (dirKeeper*)malloc(sizeof(dirKeeper));
				tmpnwdir->position = position;
				tmpnwdir->viewtop = viewtop;
				tmpnwdir->viewbottom = viewbottom;
				tmpnwdir->windowstart = windowstart;
				tmpnwdir->windowend = windowend;
				memcpy(tmpnwdir->folderpath,folderpath,50); //tmpnwdir.folderpath = folderpath;
				tmpnwdir->previous = backtrack;
				backtrack = tmpnwdir;

				//Now, actually change the folder
				append(folderpath,thefst[position].filename,50);
				append(folderpath,"/",50);
				position++;
				windowstart = position;
				windowend = thefst[position-1].filelength-1;
				viewtop = position;
				findvisible(thefst, windowstart, windowend);
				GC_Video_ClearFrameBuffer (FrameBuffer, 0x00800080);
				draw_string("Gamecube DVD Disc Browser",(320 - get_string_width("Gamecube DVD Disc Browser")/2),32);
				draw_string(folderpath,(320 - get_string_width(folderpath)/2),52);
				GC_Video_WaitForVBlank();
			}
			else if((thefst[position].filelength <= 0xC00000) && isadx(thefst[position].filename)){ //Possibly ADX file
				drawbox(170,190,300,100);
				draw_string("Playing ADX...",(320 - get_string_width("Playing ADX...")/2),220);
				draw_string("(Press B to stop)",(320 - get_string_width("(Press B to stop)")/2),244);
				GC_Video_WaitForVBlank();

				temp = DVDReadRaw((void*)0xC0A00000, thefst[position].filestart, thefst[position].filelength);
				temp = startadx((void*)0xC0A00000);

				if(temp != -1){
					mychar = 1; //1 - Playing, 0 - Stopped
					while(mychar == 1){
						if(AUDIO_DMA_LEFT < 275){
							if(update_adx() == 1){
								while(AUDIO_DMA_LEFT > 25);
								StopSample();
								mychar=0;
							}
						}
						GC_PAD_Read (&gc_Pad, GC_PAD1);
						if(gc_Pad.button & PAD_B){
							StopSample();
							mychar=0;
						}
					}
				}
				else{
					drawbox(170,190,300,100);
					draw_string("Not ADX.",(320 - get_string_width("Not ADX.")/2),228);
					draw_string("(Press B to go back)",(320 - get_string_width("(Press B to go back)")/2),244);
					GC_Video_WaitForVBlank();

					while(!(gc_Pad.button & PAD_B)){
						GC_PAD_Read (&gc_Pad, GC_PAD1);
					}
				}
			}
			while((gc_Pad.button & PAD_A) || (gc_Pad.button & PAD_B))
				GC_PAD_Read (&gc_Pad, GC_PAD1);
		}
		else if(gc_Pad.button & PAD_Y){
			char tempstring[50];
			drawbox(100,140,440,200);
			draw_string("Filename:",120,150);
			draw_string(thefst[position].filename,250,150);
			draw_string("File Start:",120,180);
			hextostring(thefst[position].filestart,tempstring);
			draw_string(tempstring,300,180);
			draw_string("File Length:",120,210);
			hextostring(thefst[position].filelength,tempstring);
			draw_string(tempstring,300,210);
			draw_string("AR Pointer Start:",120,240);
			draw_string("42000038",300,240);
			hextostring(((position*12)+16)/2,tempstring);
			draw_string(tempstring,300,262);

			draw_string("Press B to go back",(320 - get_string_width("Press B to go back")/2),310);

			GC_Video_WaitForVBlank();
			while(!(gc_Pad.button & PAD_B)){
				GC_PAD_Read (&gc_Pad, GC_PAD1);
			}
			while((gc_Pad.button & PAD_A) || (gc_Pad.button & PAD_B))
				GC_PAD_Read (&gc_Pad, GC_PAD1);
			fillblack(100,140,440,200);
		}
		else if(gc_Pad.button & PAD_B){
			if(backtrack != NULL){
				windowstart = backtrack->windowstart;
				windowend = backtrack->windowend;
				viewtop = backtrack->viewtop;
				viewbottom = backtrack->viewbottom;
				position = backtrack->position;
				memcpy(folderpath,backtrack->folderpath,50);
				dirKeeper *tmpDelDir = backtrack;
				backtrack = backtrack->previous;
				free(tmpDelDir); //Free up this ram
			}
			else{
				windowstart = 0;
				position = 0;
				viewtop = 0;
				windowend = fstfiles;
				folderpath[0] = '/';	folderpath[1] = 0;
			}
			findvisible(thefst, windowstart, windowend);
			while(gc_Pad.button & PAD_B)
				GC_PAD_Read (&gc_Pad, GC_PAD1);
			GC_Video_ClearFrameBuffer (FrameBuffer, 0x00800080);
			draw_string("Gamecube DVD Disc Browser",(320 - get_string_width("Gamecube DVD Disc Browser")/2),32);
			if(backtrack != NULL)
				draw_string(folderpath,(320 - get_string_width(folderpath)/2),52);
			else
				draw_string(gamename,(320 - get_string_width(gamename)/2),52);
			GC_Video_WaitForVBlank();
		}
		
		currentfile = viewtop;
		for(temp=viewtop; temp<viewtop+16; temp++){
			if(currentfile == position){
				int spillx;
				int spilly;

				for(spillx=0; spillx<320;spillx++){
					for(spilly=76+((temp-viewtop)*22);spilly<98+((temp-viewtop)*22);spilly++){
						FrameBuffer[(320*spilly)+spillx] = 0x0BB20B77;
					}
				}
			}
			else{
				int spillx;
				int spilly;

				for(spillx=0; spillx<320;spillx++){
					for(spilly=76+((temp-viewtop)*22);spilly<98+((temp-viewtop)*22);spilly++){
						FrameBuffer[(320*spilly)+spillx] = 0x00800080;
					}
				}
			}
			draw_string(thefst[currentfile].filename, 40, 80+(22*(temp-viewtop)));
			//1 is a folder, 2 is a file
			if(thefst[currentfile].filestart < 0x1000){
				drawchar(1,20,78+(22*(temp-viewtop)));
				currentfile = thefst[currentfile].filelength-1;
			}
			else{
				drawchar(2,20,78+(22*(temp-viewtop)));
				currentfile++;
			}
			if(currentfile == windowend)
				break;
		}
		GC_Video_WaitForVBlank();//End of one frame
		GC_PAD_Read (&gc_Pad, GC_PAD1); //Read the pad again
		if(gc_Pad.button & (PAD_UP | PAD_DOWN)){
			//They're using the Dpad instead of the analog stick.
			//Wait for another vblank to slow it down.
			GC_Video_WaitForVBlank();
		}
	}

	//The lid is open, let's free up memory.
	while(backtrack != NULL){
		dirKeeper *tmpDelDir = backtrack;
		backtrack = backtrack->previous;
		free(tmpDelDir); //Free up this ram
	}
}

void findvisible(FST_entry myfst[], int windowstart, int windowend){
	int skipto = windowstart;
	int temp;

	for(temp=0; temp<fstfiles; temp++){
		myfst[temp].visible = 0;
	}
	for(temp=windowstart; temp<windowend; temp++){
		if(temp < skipto)
			myfst[temp].visible = 0;
		else
			myfst[temp].visible = 1;

		if((temp >= skipto) && (myfst[temp].filestart < 0x1000))
			skipto = myfst[temp].filelength-1;
	}
}

