/*-----------------------------------------------------------------------------*/
// Google TV (Gen 2) DirectFB based Recovery
// by GTVHacker / cj_000 - 2012/2013
// http://www.gtvhacker.com
// cj_000@gtvhacker.com
// 
// Please, modify as you wish, and share any improvements with the community
// I'm sure there will be improvements, this was thrown together quite sloppily
// Loosely Based off of the "df_fire.c" directfb example.
//
// Features:
//		- No RSA Signature Check
//		- Root access via ADB / telnet
//		- Limited Error Checking
//		
// Enjoy!
// CJ
/*-----------------------------------------------------------------------------*/

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <directfb.h>
#include <linux/ioctl.h>
#include <fcntl.h>
#include <linux/reboot.h> 
#include <sys/mount.h>

/*
 * (Globals)
 */
static IDirectFB *dfb = NULL;
static IDirectFBSurface *primary = NULL;
static IDirectFBEventBuffer *events  = NULL;
static int screen_width  = 0;
static int screen_height = 0;
IDirectFBImageProvider *provider;
static IDirectFBSurface *logo = NULL;

#define DFBCHECK(x...)                                         \
  {                                                            \
    DFBResult err = x;                                         \
                                                               \
    if (err != DFB_OK)                                         \
      {                                                        \
        fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
        DirectFBErrorFatal( #x, err );                         \
      }                                                        \
  }


static void
exit_application( int status )
{

	primary->Release (primary);
	dfb->Release (dfb);

	/* Terminate application. */
	exit( status );
}

static void wait_to_exit(){

	DFBResult ret;
	DFBInputEvent event;
	ret = dfb->CreateInputEventBuffer( dfb, DICAPS_KEYS, DFB_FALSE, &events );
	if (ret) {
		DirectFBError( "IDirectFB::CreateEventBuffer() failed", ret );
		exit_application( 4 );
	}

	while (1){
		while (events->GetEvent( events, DFB_EVENT(&event) ) == DFB_OK) {
		       // Handle key press events. 
		       if (event.type == DIET_KEYPRESS) {
			    switch (event.key_symbol) {
				 case DIKS_SMALL_Q:
				      return;
				 case DIKS_CAPITAL_Q:
				      return;
				// back button / backspace?
			    }
		       }
		  }
	}

}

static void background_image(){

	DFBSurfaceDescription dsc;
	dsc.flags = DSDESC_CAPS;
	dsc.caps  = DSCAPS_PRIMARY | DSCAPS_FLIPPING;
	DFBCHECK (dfb->CreateImageProvider (dfb, "/assets/gtvhacker.png", &provider));
	DFBCHECK (provider->GetSurfaceDescription (provider, &dsc));
	DFBCHECK (dfb->CreateSurface( dfb, &dsc, &logo ));
	DFBCHECK (provider->RenderTo (provider, logo, NULL));
	provider->Release (provider);
	DFBCHECK (primary->Blit (primary, logo, NULL, 800, 200));
	logo->Release (logo);
}

static void screen_topper()
{
	DFBCHECK (primary->SetColor (primary, 0x00, 0x00, 0x00, 0x00));
	DFBCHECK (primary->FillRectangle (primary, 0, 0, screen_width, screen_height));
	DFBCHECK (primary->SetColor (primary, 0xFF, 0xFF, 0xFF, 0xFF));
	DFBCHECK (primary->DrawString (primary, "Google TV Custom Recovery", -1, 640,  90, DSTF_CENTER));
	background_image();
}


static void screen_write(char *text[1024])
{
	screen_topper();
	DFBCHECK (primary->DrawString (primary, text, -1, 50,  210,  DSTF_TOP | DSTF_LEFT ));
	DFBCHECK (primary->Flip (primary, NULL, DSFLIP_WAITFORSYNC));
}



static void usb_update()
{

	printf("Starting USB Update mode\r\n");
	//screen_write("---USB UPDATE MODE---");

	// You know, I should probably log this...

	/* Mounting usb (sda1, as vfat for now) */
	printf("Mounting usb (sda1, vfat, hopefully)\r\n");

	//lets see if it's already mounted
/*	FILE * testmount;
	if(fopen("/sdcard/update.zip", "rb")){
		printf("USB seems to already be mounted, lets go on!\r\n");
	}else{
		int ret;
		ret=mount("/dev/block/sda1", "/sdcard", "vfat", NULL, NULL);
		if (ret == EBUSY) {
			printf("Can't mount USB!\r\n");
			screen_write("[E]: Can't mount USB!");
			sleep (5);
			return;
		}
	}
	fclose(testmount);
*/
	/* Checking for /sdcard/update.zip (/sdcard should automount, hopefully) */
	printf("Checking for /sdcard/update.zip\r\n");

	FILE * updatesdcard;
	FILE * updatetmp;
	FILE * scrambled;
	size_t read;
	const char* binary;

	if(updatesdcard = fopen("/sdcard/update.zip", "rb")){
		/* File Found! Check if it's actually a zip, if so copy it to /tmp */
		printf("Copying /sdcard/update.zip to /tmp\r\n");
 		screen_write("[I]: Found update.zip. Copying...");
		updatetmp = fopen("/tmp/update.zip", "w+r");
		char* buffer = (char*)malloc(BUFSIZ);
		/* Read from sdcard, write to tmp/update.zip */
		while ((read = fread(buffer, 1, BUFSIZ, updatesdcard)) > 0) {
			if (fwrite(buffer, 1, read, updatetmp) != read) {
				printf("Error on copy!\r\n");
			return;
			}
		}
	
		free(buffer);
		fclose(updatesdcard);
		fclose(updatetmp);
		printf("Copy complete!\r\n");

	}else{
		/* Error checking if not found - throw an error, kick to menu */
		printf("Error! /sdcard/update.zip not found!\r\n");
	
		// Color this in red maybe - assuming I change below to the color red!
		screen_write("[E]: update.zip not found on USB!");
		sleep(5);
		return;
	}

	/* Right here is where we "should" validate that it's properly signed, but pfft... */
	printf("Currently not validating /tmp/update.zip\r\n");

	/* Extract /system/build.prop , META-INF/updater-binary */
	printf("Extracting /system/build.prop, and updater-binary to /tmp\r\n");

	// Look, I didn't want to cheat, but two lines to do this is much much eaiser than doing it the correct way, all for 3 files.
	// Sure, it's not the safe way to do it, but telnet is wide open, not like we are trying to keep anyone out.
	//system("echo A | /bin/unzip /tmp/update.zip META-INF/com/google/android/update-binary scrambled.prop system/build.prop -d/tmp/");
	// Going to need to this (i mean, we could run it 2-3 times, but that's quite lame, ideally it just needs to work like busybox's, but for now lets do this:

	system("/bin/miniunz -x -o /tmp/update.zip system/build.prop -d /tmp/");

	/* This is on a Sony right now, updates are scrambled (check the wiki for more info). Simpler to do it with a decrypted binary that */ 
	/* will handle the decryption then sorting the scrambling, and adding it all in! */
	binary = "/bin/update-binary-sony";

	//if(scrambled = fopen("/tmp/scrambled.prop", "r")){
	//	printf("scrambled.prop detected, assuming scrambled files. Fallback to a decrypted update-binary!\r\n");
	//	binary = "/tmp/META-INF/com/google/android/update-binary";
	//}else{
	//	binary = "/bin/update-binary-sony";
	//}

	/* Confirm this is for the right device (match build.prop in update against our build.prop) */
	printf("Checking if this is the right device\r\n");

	/* Lets just look at /tmp/build.prop to start - cheating, but we can go from there, by mounting /system and checking /system/build.prop */

/*	FILE *filesys;
	filesys = fopen("/system/build.prop","r");
	if(filesys == NULL){
		printf("ERR!!!");
		screen_write("Error! Press Q to go back!");
		wait_to_exit();
	}

	char* line[1024];
	char newline[1024];
	while (fgets(line, sizeof line, filesys) != NULL){
		if(strstr(line,"ro.product.device=")){
			printf(line);
			sprintf(newline,"%s",line);
		}
	}
	fclose(filesys);

	FILE *filetmp;
	filetmp = fopen("/tmp/system/build.prop","r");
	if(filetmp == NULL){
		printf("ERR!!!");
		screen_write("[E]: Bad Update! Q to go back");
		wait_to_exit();
	}

	char linetmp[1024];
	while (fgets(linetmp, sizeof linetmp, filetmp) != NULL){
		if(strstr(linetmp,"ro.product.device=")){
			linetmp[(strlen(linetmp)-1)]=0;
			newline[(strlen(newline)-1)]=0;
			if(strcmp(linetmp,newline) == 0){
				printf("Update product matched installed product! \r\n");
			}else{
				screen_write("[E]: Wrong Product!");
				wait_to_exit();
				return;
			}

		}
	}
	fclose(filetmp);
*/
	/* Assuming no errors - pass the zip to updater-binary, and sit back */
	printf("Passing control to updater-binary\r\n");
	screen_write("[I]: Starting update process.");

/* Swiped and modified from install.c / AOSP  */
/* Fun Fact: You can tell the parts that were modified by the really bad code! */

	const char* path = "/tmp/update.zip";
	// IF this based off scrambled!

	int pipefd[2];
	pipe(pipefd);

	// When executing the update binary contained in the package, the
	// arguments passed are:
	//
	//   - the version number for this interface
	//
	//   - an fd to which the program can write in order to update the
	//     progress bar.  The program can write single-line commands:
	//
	//        progress <frac> <secs>
	//            fill up the next <frac> part of of the progress bar
	//            over <secs> seconds.  If <secs> is zero, use
	//            set_progress commands to manually control the
	//            progress of this segment of the bar
	//
	//        set_progress <frac>
	//            <frac> should be between 0.0 and 1.0; sets the
	//            progress bar within the segment defined by the most
	//            recent progress command.
	//
	//        firmware <"hboot"|"radio"> <filename>
	//            arrange to install the contents of <filename> in the
	//            given partition on reboot.
	//
	//            (API v2: <filename> may start with "PACKAGE:" to
	//            indicate taking a file from the OTA package.)
	//
	//            (API v3: this command no longer exists.)
	//
	//        ui_print <string>
	//            display <string> on the screen.
	//
	//   - the name of the package zip file.
	//

	/* Make our updater executable */ 
	chmod(binary,0777);

	const char** args = (const char**)malloc(sizeof(char*) * 5);
	args[0] = binary;
	args[1] = "3";   // ?? defined in Android.mk
	char* temp = (char*)malloc(10);
	sprintf(temp, "%d", pipefd[1]);
	args[2] = temp;
	args[3] = (char*)path;
	args[4] = NULL;

	pid_t pid = fork();
	if (pid == 0) {
	close(pipefd[0]);
	execv(binary, (char* const*)args);
	fprintf(stdout, "E:Can't run %s (%s)\n", binary, strerror(errno));
	screen_write("[E]: Cant run update-binary!");
	_exit(-1);
	}
	close(pipefd[1]);

	char buffer[1024];
	FILE* from_child = fdopen(pipefd[0], "r");
	while (fgets(buffer, sizeof(buffer), from_child) != NULL) {
	char* command = strtok(buffer, " \n");
	if (command == NULL) {
	    continue;
	} else if (strcmp(command, "progress") == 0) {
	    char* fraction_s = strtok(NULL, " \n");
	    char* seconds_s = strtok(NULL, " \n");
	    float fraction = strtof(fraction_s, NULL);
	    int seconds = strtol(seconds_s, NULL, 10);
            char* percent[255];
	    int frac=fraction*100;
	    sprintf(percent, "[I]: %d%% completed",frac);
	    printf("%s\r\n", percent);
	    screen_write(percent);
	    sleep(2);

  	    // TO DO: Add a bar or moving thingy, or something for the future! - CJ

	} else if (strcmp(command, "set_progress") == 0) {
	    char* fraction_s = strtok(NULL, " \n");
	    float fraction = strtof(fraction_s, NULL);
	} else if (strcmp(command, "ui_print") == 0) {
	    char* str = strtok(NULL, "\n");
	    if (str) {
           	char* msg[255];
		sprintf(msg, "[I]: %s", str);	
		screen_write(msg);
		printf("%s \r\n", msg);
	   	sleep(2);

	    } 
	} else {
	    printf("unknown command [%s]\n", command);
	}
	}
	fclose(from_child);

	int statusaosp;
	waitpid(pid, &statusaosp, 0);

/* Swiped and modified from install.c / AOSP */

	if(WIFEXITED(statusaosp)){
		printf("Exit Status: %d \r\n",WEXITSTATUS(statusaosp));
		screen_write("Completed! Press Q to go back");
	}else{
		screen_write("There may have been an error");
	}
	
	wait_to_exit();	
}

static void factory_reset()
{
	/* Wipe /data */
	printf("Factory Reset\r\n");
	screen_write("Factory Reset");
	return;
}

static void system_information()
{
	/* Get the latest software version and print it out to the screen. Maybe mac addys too? */
	printf("System Information\r\n");

	char *getrecver="Recovery Version: 1.1";
	char gethwtype[1025];
	char getregion[1025];
	char getver[1025];
	char getdisplay[1025];

	/* Lets just look at /tmp/build.prop to start - cheating, but we can go from there, by mounting /system and checking /system/build.prop */
	FILE *file;
	file = fopen("/system/build.prop","r");
	if(file == NULL){
		printf("ERR!!!");
		screen_write("Error! Press Q to go back!");
		wait_to_exit();
	}

	char* line[1024];
	while (fgets(line, sizeof line, file) != NULL){
		if(strstr(line,"ro.build.product")){
			char *ptr=strtok(line,"=");
			sprintf(gethwtype,"HW: %s",strtok(NULL,"="));
			gethwtype[(strlen(gethwtype)-1)]=0;
		}

	//	if(strstr(line,"ro.com.sony.btv.wlan.region")){
	//		char *ptr=strtok(line,"=");
	//		sprintf(getregion,"Region: %s",strtok(NULL,"="));
	//		getregion[(strlen(getregion)-1)]=0;
	//	}

		if(strstr(line,"ro.build.version.release")){
			char *ptr=strtok(line,"=");
			sprintf(getver,"Version: %s",strtok(NULL,"="));
			getver[(strlen(getver)-1)]=0;
		}

		if(strstr(line,"ro.build.display")){
			char *ptr=strtok(line,"=");
			sprintf(getdisplay,"%s",strtok(NULL,"="));
			getdisplay[(strlen(getdisplay)-1)]=0;
			// this is hard coded a bit for now, should fix in the future
		}
	}

	fclose(file);


//	sprintf(rootstatus, "Root Stauts: %s", "4");	

	screen_topper();
	DFBCHECK (primary->DrawString (primary, getrecver, -1, 40,  160,  DSTF_TOP | DSTF_LEFT ));
	DFBCHECK (primary->DrawString (primary, gethwtype, -1, 40,  230,  DSTF_TOP | DSTF_LEFT ));
	DFBCHECK (primary->DrawString (primary, getregion, -1, 40,  300,  DSTF_TOP | DSTF_LEFT ));
	DFBCHECK (primary->DrawString (primary, getver, -1, 40,  370,  DSTF_TOP | DSTF_LEFT ));
	DFBCHECK (primary->DrawString (primary, getdisplay, -1, 40,  500,  DSTF_TOP | DSTF_LEFT ));
//	DFBCHECK (primary->DrawString (primary, rootstatus, -1, 40,  430,  DSTF_TOP | DSTF_LEFT ));
	DFBCHECK (primary->DrawString (primary, "Press Q to go back", -1, 40,  560,  DSTF_TOP | DSTF_LEFT ));

	DFBCHECK (primary->Flip (primary, NULL, DSFLIP_WAITFORSYNC));

	wait_to_exit();

}

static void reboot_prog()
{
	/* Reboot! */
	printf("Reboot! We are going down NOW!\r\n");
	reboot(0x01234567);
	_exit(0);
	/* FTS should already be cleared by our oneshot (fixfts). */
}

static void about()
{

	printf("About\r\n");

	screen_topper();
	DFBCHECK (primary->DrawString (primary, "Recovery and exploit by cj_000.", -1, 40,  160,  DSTF_TOP | DSTF_LEFT ));
	DFBCHECK (primary->DrawString (primary, "Visit GTVHacker.com for more info", -1, 40,  230,  DSTF_TOP | DSTF_LEFT ));
	DFBCHECK (primary->DrawString (primary, "GTVHacker is:", -1, 40,  430,  DSTF_TOP | DSTF_LEFT ));
	DFBCHECK (primary->DrawString (primary, "[mbm], AgentHH, cj_000, tdweng, zenofex", -1, 40,  500,  DSTF_TOP | DSTF_LEFT ));
	DFBCHECK (primary->DrawString (primary, "Press Q to go back", -1, 40,  570,  DSTF_TOP | DSTF_LEFT ));

	DFBCHECK (primary->Flip (primary, NULL, DSFLIP_WAITFORSYNC));

	

	wait_to_exit();
}


 // The font we will use to draw the text.
static IDirectFBFont *font = NULL;

int main (int argc, char **argv)
{
 
	DFBResult ret;

	//A structure describing font properties.
	DFBFontDescription font_dsc;

	DFBSurfaceDescription dsc;

	DFBCHECK (DirectFBInit (&argc, &argv));
	DFBCHECK (DirectFBCreate (&dfb));
	DFBCHECK (dfb->SetCooperativeLevel (dfb, DFSCL_FULLSCREEN));
	dsc.flags = DSDESC_CAPS;
	dsc.caps  = DSCAPS_PRIMARY | DSCAPS_FLIPPING;
	DFBCHECK (dfb->CreateSurface( dfb, &dsc, &primary ));
	DFBCHECK (primary->GetSize (primary, &screen_width, &screen_height));

	/*
	* First we need to create a font interface by passing a filename
	* and a font description to specify the desired font size. DirectFB will 
	* find (or not) a suitable font loader.
	*/
	font_dsc.flags = DFDESC_HEIGHT;
	font_dsc.height = 48;
	DFBCHECK (dfb->CreateFont (dfb, "/assets/FreeSans.ttf", &font_dsc, &font));

	//Set the font to the surface we want to draw to.

	DFBCHECK (primary->SetFont (primary, font));

	ret = dfb->CreateInputEventBuffer( dfb, DICAPS_KEYS, DFB_FALSE, &events );
	if (ret) {
	  DirectFBError( "IDirectFB::CreateEventBuffer() failed", ret );
	  exit_application( 4 );
	}
     
	/* Main loop. */
	while (1) {

	screen_topper();

	//DFBCHECK (primary->DrawString (primary, "1) Install update from USB", -1, 40,  210,  DSTF_TOP | DSTF_LEFT ));
	DFBCHECK (primary->DrawString (primary, "1) Install Update from USB", -1, 40,  210,  DSTF_TOP | DSTF_LEFT ));
	DFBCHECK (primary->DrawString (primary, "2) Factory Reset (Soon)", -1, 40,  260,  DSTF_TOP | DSTF_LEFT ));
	DFBCHECK (primary->DrawString (primary, "3) System Information", -1, 40,  310,  DSTF_TOP | DSTF_LEFT ));
	DFBCHECK (primary->DrawString (primary, "4) About", -1, 40,  360,  DSTF_TOP | DSTF_LEFT ));
	DFBCHECK (primary->DrawString (primary, "5) Reboot", -1, 40,  410,  DSTF_TOP | DSTF_LEFT ));

	DFBCHECK (primary->Flip (primary, NULL, DSFLIP_WAITFORSYNC));


	 DFBInputEvent event;

	  // Check for new events. 
	  while (events->GetEvent( events, DFB_EVENT(&event) ) == DFB_OK) {

	       // Handle key press events. 
	       if (event.type == DIET_KEYPRESS) {
		    switch (event.key_symbol) {
		         case DIKS_ESCAPE:
		         case DIKS_POWER:
		         case DIKS_BACK:
		         case DIKS_SMALL_Q:
		         case DIKS_CAPITAL_Q:
		              exit_application( 0 );
		              break;
		         case DIKS_1:
		              usb_update();
		              break;
		         case DIKS_2:
		              factory_reset();
		              break;
		         case DIKS_3:
		              system_information();
		              break;
		         case DIKS_4:
		              about();
		              break;
		         case DIKS_5:
		              reboot_prog();
		              break;


		         default:
		              break;
		    }
	       }
	  }

	}



	sleep(1);

	font->Release (font);
	primary->Release (primary);
	dfb->Release (dfb);

	return 23;
}
