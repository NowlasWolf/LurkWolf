#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/ip.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<netdb.h>
#include<pthread.h>
#include <ncurses.h>
#include <signal.h>
#include<ctype.h>
#include<time.h>

#include "lurk.h"

#define OTHER_MAX 64
struct receive_info { //Struct for sending most windows and socket
	int skt;
	WINDOW* input;
	WINDOW* log;
	WINDOW* header;
	WINDOW* pstatus;
	WINDOW* inputwin;
	WINDOW* extrawindow1;
	WINDOW* extrawindow2;
};


struct input_info { //Struct that is used mostly for input
	WINDOW* log;
	WINDOW* inputwin;
};

//Mutex Lock

// A variety of global variables
int skt;
float version = 1.06;
char ip[64];
char port[8];
char name[32];
int max_x, max_y, log_x, log_y;
int new_x, new_y;
bool connected, started;
int points;
int lastroom = 0;
bool verbose = false;
bool characcept = false;
bool quickmsg = false;
bool changetext = true;
bool leftroom = false;

//Extra Window Flags
bool extrawin;
bool extrawin1detail = true;
bool extrawin2detail = true;
bool showinactive1 = true;
bool showinactive2 = true;
int extrawin1mode = 0;
int extrawin2mode = 1;
int extrawin1advance = 0;
int extrawin2advance = 0;

//Counters for struct arrays
int playercount = 1;
int connectioncount = 1;
int enemycount = 0;
int friendcount = 0;



char pastcommand[100];

//Struct arrays for storing stuff in current room
//struct character players[OTHER_MAX];
//struct character enemies[OTHER_MAX];
//struct room rooms[OTHER_MAX];
//struct character friends[OTHER_MAX];

const size_t charactersize = (sizeof(struct character) * OTHER_MAX);
const size_t roomsize = sizeof(struct room) * OTHER_MAX;

struct character* players;
struct character* enemies;
struct room* rooms;
struct character* friends;

void newgetstr(WINDOW* win, char* in){
	int c;
	int xpos = 0, ypos = 0, homex = 0, len = 0;
	getyx(win,ypos,homex);
	xpos = homex;
	wmove(win,0,1);
	for(;;){
		//in[xpos] = '\0';
		wmove(win,ypos,xpos);
		c = getch();
		wprintw(win, "%c",c);
		wrefresh(win);
		if(c == KEY_ENTER || c == '\n' || c == '\r' || c == 410){
			wclear(win);
			wrefresh(win);
			break;
		}else if(isprint(c) && c != KEY_UP){
			//memmove(in+xpos-homex+1, in+xpos-homex, len-xpos-homex);
			in[xpos-homex] = c;
			len++;
			xpos++;
		}else if(c == KEY_UP){
			if(!(pastcommand[0] == 0)){
				strncpy(in, pastcommand, 100);
				len = strlen(in);
				xpos = strlen(in)+1;
				wmove(win, ypos, xpos);
				mvwprintw(win, 0, 1, in);
				wrefresh(win);
			}
		}
		/*else if(c == KEY_LEFT ){
			if(xpos > homex){
				xpos -= 1;
				wmove(win,ypos,xpos);
			}
		}else if(c == KEY_RIGHT){
			if(xpos <= len){
				xpos += 1;
				wmove(win,ypos,xpos);			
			}
		}*/else if(c == KEY_BACKSPACE){
			xpos -= 1;
			if(xpos < homex){
				xpos += 1;
				wmove(win,ypos,xpos);
				wrefresh(win);	
			}		
			if(xpos >= homex){
				len -= 1;
				if (len < 0){
					len = 0;				
				}
				wmove(win,ypos,xpos);
				wprintw(win, " ");
				wrefresh(win);				
				wmove(win,ypos,xpos);
			}
		}
	}
	in[len] = 0;
	if(in[0] == '/'){
		strncpy(pastcommand,in, 100);
	}
	
}

//Prompts the user in the input window with provided string and returns answer in input buffer
char* request(WINDOW* in, char* enter, char* input){
	mvwprintw(in,0,0,"%s",enter);
	wgetstr(in, input);
	//newgetstr(in, input);
	wclear(in);
	return input;
}

//Normal get input
void get_input(void* arg, char* input){
	struct input_info* ii = (struct input_info*)arg;
	//mvwprintw(ii->inputwin,0,0,">");
	mvwaddstr(ii->inputwin,0,0,">");
	wmove(ii->inputwin,0,1);
	wrefresh(ii->inputwin);
	wgetstr(ii->inputwin, input);
	//newgetstr(ii->inputwin, input);
	wprintw(ii->log,"<%s> %s\n", players[0].name, input);
	wrefresh(ii->log);
	wclear(ii->inputwin);
		
}

//Handle the CTRL+C signal
void handle_signal(int signal){
	if (signal == SIGINT){
		printf("\nClosing Nicely\n");
		endwin();
		close(skt);
		exit(0);	
	}
}

//Draws the header window with the program name, status, and extra window 1 mode
void update_header(WINDOW *win){
	wclear(win);
	mvwprintw(win,0,0, "LurkWolf Client %.2f",version); // Program Title
	mvwprintw(win,0,max_x/2-22,"Status: "); // Program Status(just used for showing connected/disconnected + the IP and port)
	if(connected){
		wattron(win,COLOR_PAIR(1));		
		wprintw(win,"Connected: %s %s", ip, port);
		wattroff(win,COLOR_PAIR(1));
	} else {
		wattron(win,COLOR_PAIR(2));
		wprintw(win,"Disconnected");
		wattroff(win,COLOR_PAIR(2));
	}

	if(extrawin){ // Extra Window 1 Mode display
		char windowname[8];
		if(extrawin1mode == 0) strcpy(windowname, "Players"); 
		else if(extrawin1mode == 1) strcpy(windowname, "Enemies");
		else if(extrawin1mode == 2) strcpy(windowname, "Rooms  ");
		else if(extrawin1mode == 3) strcpy(windowname, "Friends");
		mvwprintw(win,0,max_x-25, "Window1: %s", windowname);
	}
	for(int i=0 ; i<max_x; i++){ //Border!
		mvwprintw(win,1,i,"-");
	}
	wnoutrefresh(win);
}

//Draws the status window that is in between the log and input window, has useful player info and the extra window 2 mode
void update_pstatus(WINDOW* win){
	wclear(win);
	for(int i=0 ; i<max_x; i++){ //Border!
		mvwprintw(win,0,i,"-");
		mvwprintw(win,2,i,"-");
	}
	if(characcept){ //Start showing stuff after an accept code 10 is received from server
		mvwprintw(win,1,0,"%s",players[0].name);
		wprintw(win," | GOLD:%d",players[0].gold);
		if(getbit(7,players[0].flags)) wprintw(win," | HP:%d",players[0].health);
		else wprintw(win," | Dead");
		wprintw(win," | ATK:%d",players[0].attack);
		wprintw(win," | DEF:%d",players[0].defence);
		wprintw(win," | REG:%d",players[0].regen);
		if(getbit(6,players[0].flags)){
			wprintw(win," | JoinB[X]");
		}
		else{
			wprintw(win," | JoinB[ ]");
		}
		if(started){
			wprintw(win," | CRM:%s",rooms[0].name);
		}
	}
	if(extrawin){ // Extra Window 2 Mode display
		char windowname[8];
		if(extrawin2mode == 0) strcpy(windowname, "Players");
		else if(extrawin2mode == 1) strcpy(windowname, "Enemies");
		else if(extrawin2mode == 2) strcpy(windowname, "Rooms  ");
		else if(extrawin2mode == 3) strcpy(windowname, "Friends");
		mvwprintw(win,1,max_x-25, "Window2: %s", windowname);
	}
	
	wnoutrefresh(win);
}

//Draws the extra windows according to the flags. It's a mess but it's my mess
void update_extrawin(WINDOW* win1, WINDOW* win2){
	if(extrawin){ // If extra windows is enabled
		WINDOW* exwins[2] = {win1, win2};
		int exwin_x, exwin_y, win1_y, advance;
		bool extradetail,showinactive;
		for(int n=0; n < 2; n++){
			wclear(exwins[n]); // Clear windows of old stuff
			getmaxyx(exwins[n],exwin_y,exwin_x);
			if(n == 0){			//Sets things up for what to print for each window so I can just reuse code in a loop
				win1_y = exwin_y;
				advance = extrawin1advance;
				extradetail = extrawin1detail;
				showinactive = showinactive1;
			}
			if(n == 1){
				advance = extrawin2advance;
				extradetail = extrawin2detail;
				showinactive = showinactive2;
			}
			if(extrawin1mode == extrawin2mode){
				advance = extrawin1advance;
				extradetail = extrawin1detail;
			}			
			if(started){ //If the user is started in the server
				if((extrawin1mode == 0 && n == 0) || (extrawin2mode == 0 && n == 1)){ // Window mode Player
					for(int g=advance ; g<playercount; g++){

						//Code dealing with extending stuff out to win2 and stopping content at and and telling how much more is left
						if((extrawin1mode != extrawin2mode) && g-advance >= exwin_y-1){
							wprintw(exwins[n]," %d more out of %d", playercount - g , playercount);
							break;
						}
						if((extrawin1mode != extrawin2mode) && extradetail && g-advance >= win1_y/2-1){
							wprintw(exwins[n]," %d more out of %d", playercount - g , playercount);
							break;
						}
						int i = g;
						if((extrawin1mode == extrawin2mode) && n == 0 && g-advance  >= win1_y){
							break;
						}
						if((extrawin1mode == extrawin2mode) && extradetail && n == 0 && g-advance  >= win1_y/2){
							break;
						}
						if((extrawin1mode == extrawin2mode) && extradetail && n == 1 && g-advance  >= win1_y/2){
							wprintw(exwins[n]," %d more out of %d", playercount - g - g , playercount);
							break;
						}
						if((extrawin1mode == extrawin2mode) && n == 1 && g-advance  >= win1_y){
							wprintw(exwins[n]," %d more out of %d", playercount - g - g, playercount);
							break;
						}
						if((extrawin1mode == extrawin2mode) && n == 1 && extrawin1detail == true && win1_y/2+g-advance  >= win1_y/2){
							i = win1_y/2 + g;
						}
						if((extrawin1mode == extrawin2mode) && n == 1 && extrawin1detail == false && win1_y+g-advance  >= win1_y){
							i = win1_y + g;
						}
						if(i>=playercount)break;
						
						//Actual Print Stuff
						if(showinactive){
							wprintw(exwins[n]," >%s\n",players[i].name);
							if(extradetail && getbit(7,players[i].flags) && getbit(4,players[i].flags)) wprintw(exwins[n],"  HP %d|ATK %d|DEF %d|REG %d\n",players[i].health,players[i].attack,players[i].defence,players[i].regen);
							else if(extradetail && ((!getbit(7,players[i].flags) && players[i].health > 0) || !getbit(4,players[i].flags))) wprintw(exwins[n],"  ~~Inactive~~\n",players[i].gold);
							else if(extradetail && !getbit(7,players[i].flags)) wprintw(exwins[n],"  RIP Dead | Gold: %d\n",players[i].gold);
						}
						else if(getbit(4,players[i].flags) && getbit(7,players[i].flags) || (players[i].health <= 0 && !getbit(7,players[i].flags))) {
							wprintw(exwins[n]," %s\n",players[i].name);
							if(extradetail && getbit(7,players[i].flags)) wprintw(exwins[n],"  HP %d|ATK %d|DEF %d|REG %d\n",players[i].health,players[i].attack,players[i].defence,players[i].regen);
							//else if(extradetail && (!getbit(7,players[i].flags || !getbit(4,players[i].flags))) && players[i].health > 0) wprintw(exwins[n],"  ~~Inactive~~\n",players[i].gold);
							else if(extradetail && !getbit(7,players[i].flags)) wprintw(exwins[n],"  RIP Dead | Gold: %d\n",players[i].gold);
						}
					
					}
				}else if((extrawin1mode == 1 && n == 0) || (extrawin2mode == 1 && n == 1)){ //Window Mode Enemy
					for(int g=advance ; g<enemycount; g++){
						
						//Code dealing with extending stuff out to win2 and stopping content at and and telling how much more is left
						if((extrawin1mode != extrawin2mode) && g >= exwin_y-1){
							wprintw(exwins[n]," %d more out of %d", enemycount - g, enemycount);
							break;
						}
						if((extrawin1mode != extrawin2mode) && extradetail && g-advance >= win1_y/2-1){
							wprintw(exwins[n]," %d more out of %d", enemycount - g, enemycount);
							break;
						}
						int i = g;
						if((extrawin1mode == extrawin2mode) && n == 0 && g-advance >= win1_y){
							break;
						}
						if((extrawin1mode == extrawin2mode) && extradetail && g-advance >= win1_y/2){
							break;
						}
						if((extrawin1mode == extrawin2mode) && extradetail && n == 1 && g-advance  >= win1_y/2){
							wprintw(exwins[n]," %d more out of %d", enemycount - g - g , enemycount);
							break;
						}
						if((extrawin1mode == extrawin2mode) && n == 1 && g-advance  >= win1_y){
							wprintw(exwins[n]," %d more out of %d", enemycount - g - g, enemycount);
							break;
						}
						if((extrawin1mode == extrawin2mode) && n == 1 && extrawin1detail == true && win1_y/2+g-advance >= win1_y/2){
							i = win1_y/2 + g;
						}
						if((extrawin1mode == extrawin2mode) && n == 1 && extrawin1detail == false && win1_y+g-advance >= win1_y-1){
							i = win1_y + g;
						}
						if(i>=enemycount)break;
						
						//Actual Print Stuff
						wprintw(exwins[n]," >%s\n",enemies[i].name);
						if(extradetail && getbit(7,enemies[i].flags)) wprintw(exwins[n],"  HP %d|ATK %d|DEF %d|REG %d\n",enemies[i].health,enemies[i].attack,enemies[i].defence,enemies[i].regen);
						else if(extradetail)wprintw(exwins[n],"  RIP Dead | Gold: %d\n",enemies[i].gold);
					}
				}else if((extrawin1mode == 2 && n == 0) || (extrawin2mode == 2 && n == 1)){ //Window mode room/connection
					for(int g=advance ; g<connectioncount; g++){
						
						//Code dealing with extending stuff out to win2 and stopping content at and and telling how much more is left
						if((extrawin1mode != extrawin2mode) && g >= exwin_y-1){
							wprintw(exwins[n]," %d more out of %d", connectioncount - g, connectioncount);
							break;
						}
						if((extrawin1mode != extrawin2mode) && extradetail && g-advance >= win1_y/2-1){
							wprintw(exwins[n]," %d more out of %d", connectioncount - g, connectioncount);
							break;
						}
						int i = g;
						if((extrawin1mode == extrawin2mode) && n == 0 && g-advance >= win1_y){
							break;
						}
						if((extrawin1mode == extrawin2mode) && extradetail && g-advance >= win1_y/2){
							break;
						}
						if((extrawin1mode == extrawin2mode) && extradetail && n == 1 && g-advance  >= win1_y/2){
							wprintw(exwins[n]," %d more out of %d", connectioncount - g - g , connectioncount);
							break;
						}
						if((extrawin1mode == extrawin2mode) && n == 1 && g-advance  >= win1_y){
							wprintw(exwins[n]," %d more out of %d", connectioncount - g - g, connectioncount);
							break;
						}
						if((extrawin1mode == extrawin2mode) && n == 1 && extrawin1detail == true && win1_y/2+g-advance >= win1_y/2){
							i = win1_y/2 + g;
						}
						if((extrawin1mode == extrawin2mode) && n == 1 && extrawin1detail == false && win1_y+g-advance >= win1_y-1){
							i = win1_y + g;
						}
						if(i>=connectioncount)break;						
						
						//Actual Print Stuff
						wprintw(exwins[n]," >%s\n",rooms[i].name);
						if(extradetail) wprintw(exwins[n],"  Number: %d\n",rooms[i].number);
					}
				}
				else if((extrawin1mode == 3 && n == 0) || (extrawin2mode == 3 && n == 1)){
					for(int g=advance ; g<friendcount; g++){
						if((extrawin1mode != extrawin2mode) && g >= exwin_y-1){
							wprintw(exwins[n]," %d more out of %d", friendcount - g, friendcount);
							break;
						}
						if((extrawin1mode != extrawin2mode) && extradetail && g-advance >= win1_y/2-1){
							wprintw(exwins[n]," %d more out of %d", friendcount - g, friendcount);
							break;
						}
						int i = g;
						if((extrawin1mode == extrawin2mode) && n == 0 && g-advance >= win1_y){
							break;
						}
						if((extrawin1mode == extrawin2mode) && extradetail && g-advance >= win1_y/2){
							break;
						}
						if((extrawin1mode == extrawin2mode) && extradetail && n == 1 && g-advance  >= win1_y/2){
							wprintw(exwins[n]," %d more out of %d", friendcount - g - g , friendcount);
							break;
						}
						if((extrawin1mode == extrawin2mode) && n == 1 && g-advance  >= win1_y){
							wprintw(exwins[n]," %d more out of %d", friendcount - g - g, friendcount);
							break;
						}
						if((extrawin1mode == extrawin2mode) && n == 1 && extrawin1detail == true && win1_y/2+g-advance >= win1_y/2){
							i = win1_y/2 + g;
						}
						if((extrawin1mode == extrawin2mode) && n == 1 && extrawin1detail == false && win1_y+g-advance >= win1_y-1){
							i = win1_y + g;
						}
						if(i>=friendcount)break;
						if(showinactive1){
							wprintw(exwins[n]," >%s\n",friends[i].name);
							if(extradetail && getbit(7,friends[i].flags)) wprintw(exwins[n],"  HP %d|ATK %d|DEF %d|REG %d\n",friends[i].health,friends[i].attack,friends[i].defence,friends[i].regen);
							else if(extradetail && !getbit(7,friends[i].flags)) wprintw(exwins[n],"  RIP Dead | Gold: %d\n",friends[i].gold);
						}
						else if(getbit(4,friends[i].flags) && getbit(7,friends[i].flags) || (friends[i].health <= 0 && !getbit(7,friends[i].flags))) {
							wprintw(exwins[n]," %s\n",friends[i].name);
							if(extradetail && getbit(7,friends[i].flags)) wprintw(exwins[n],"  HP %d|ATK %d|DEF %d|REG %d\n",friends[i].health,friends[i].attack,friends[i].defence,friends[i].regen);
							else if(extradetail && !getbit(7,friends[i].flags) && friends[i].health > 0) wprintw(exwins[n],"  ~~Inactive~~\n",friends[i].gold);
							else if(extradetail && !getbit(7,friends[i].flags)) wprintw(exwins[n],"  RIP Dead | Gold: %d\n",friends[i].gold);
						}
					
					}
				
									
				}
			}		

			for(int i=0 ; i<exwin_y; i++){ //Border!
				mvwprintw(exwins[n],i,0,"|");
			}

			wnoutrefresh(exwins[n]);
		}
	}
}


void removeplayer(int playernum){
	for(int i=playernum; i<playercount-1; i++){
		memmove(&players[i],&players[i+1],sizeof(struct character));
	}
	playercount--;
}


//Server input thread. It gets stuff from server, stores it, and prints it(updating windows in the process)
void *server_handler(void *arg){
	struct receive_info* outputs = (struct receive_info*)arg;
	
	int type;
	size_t connectcheck;
	char readstring[1024*1024];
	if(verbose){
		wprintw(outputs->log, "<debug> Server thread started!\n");
		wrefresh(outputs->log);
	}
	for(;;){
		//Refresh windows and make sure type is zeroed out
		type = 0;
		update_header(outputs->header);
		update_pstatus(outputs->pstatus);
		update_extrawin(outputs->extrawindow1, outputs->extrawindow2);
		doupdate();
		//wmove(outputs->input,0,1);
		//READ FIRST BYTE
		connectcheck = read(outputs->skt, &type, 1);
		if(connectcheck==0){ //Checks to see if the connection is still around. If not then the thread ends and you are to enter in a new IP and Port
			wattron(outputs->log,COLOR_PAIR(2));
			wprintw(outputs->log,"<local> Disconnected from server\n");
			wattroff(outputs->log,COLOR_PAIR(2));
			wrefresh(outputs->log);
			characcept = false;
			started = false;
			break;

		}
		/*if(verbose){
			wprintw(outputs->log, "<debug> Receiving %d\n", type);
			wrefresh(outputs->log);
		}*/


		if(type == 13){ //CONNECTION Gets connection packets and stores them in rooms and increments connection count. Packets are checked against existing data to update if needed
			bool stored = false;
			struct room tempr = {0};
			lurk_connection(skt,1,&tempr);
			if(verbose){
				wprintw(outputs->log, "<debug> Received connection:\n",tempr.name);
				wprintw(outputs->log, "\tName: %s\n",tempr.name);
				wprintw(outputs->log, "\tDescription: %s\n",tempr.desc);
				wprintw(outputs->log, "\tNumber: %d|",tempr.number);
				wprintw(outputs->log, "DescLen: %d\n",tempr.descl);
				wrefresh(outputs->log);
			}
			for(int i=0; i<connectioncount; i++){ //Checking against existing data and updates if found
				if(!strcmp(rooms[i].name, tempr.name)){
					if(verbose){
						wprintw(outputs->log, "<debug> Found connection %s stored. Updating...\n",tempr.name);
						wrefresh(outputs->log);
					}
					rooms[i] = tempr;
					stored = true;
					break;
				}
			}
			if(!stored){ //Packet was not found in data so it saves it in the next index and increments connectioncount
				if(verbose){
					wprintw(outputs->log, "<debug> Connection %s not stored. Storing...\n",tempr.name);
					wrefresh(outputs->log);
				}
				wattron(outputs->log,COLOR_PAIR(4));
				wprintw(outputs->log, "<connection> %s: %s\n",tempr.name, tempr.desc);
				wattroff(outputs->log,COLOR_PAIR(4));
				wrefresh(outputs->log);
				rooms[connectioncount] = tempr;
				connectioncount++;
			}
			stored = false;
			
		}
		else if(type == 11){ //GAME Gets the game packet and displays it. Inital points are saved
			wattron(outputs->log,COLOR_PAIR(1));
			mvwprintw(outputs->log,log_y-1,0, "<local> Game Connected!\n");
			wattroff(outputs->log,COLOR_PAIR(1));
			wrefresh(outputs->log);
			struct game foobar = {0};
			lurk_game(skt,1,&foobar);
			points = foobar.ipoints;
			wprintw(outputs->log, "<game> %s\n",foobar.desc);
			wprintw(outputs->log, "<local> Type \"/make\" to create your character!\n");
			wprintw(outputs->log, "\tOr use \"/login\" to input a name only and use default values.\n");
			wrefresh(outputs->log);
		}
		
		else if(type == 10){//CHARACTER Gets character packets and stores them in players or enemies and increments the count for each. Packets are checked against existing data to update if needed
			struct character tempc = {0};
			lurk_character(skt,1,&tempc);
			tempc.name[31] = 0;
			for(int i = 0; i < strlen(tempc.name); i++){ //Checks name for some escape chars to clean up any bad input other clients or servers may have introduced
				if(tempc.name[i] ==  '\n' || tempc.name[i] == '\r' || tempc.name[i] == '\f'){
					if(verbose){
						wprintw(outputs -> log,"<local> escape char found in name at pos %d\n",i);
						wrefresh(outputs->log);
					}
					tempc.name[i] = ' ';
				}
			}
			tempc.desc[strlen(tempc.desc)] = 0;
			for(int i = 0; i < strlen(tempc.desc); i++){ //Checks description for some escape chars to clean up any bad input other clients or servers may have introduced
				if(tempc.desc[i] ==  '\n' || tempc.desc[i] == '\r' || tempc.desc[i] == '\f'){
					if(verbose){
						wprintw(outputs -> log,"<local> escape char found in desc at pos %d\n",i);
						wrefresh(outputs->log);
					}
					tempc.desc[i] = ' ';
				}
			}
			
			
			bool stored = false;
			if(verbose){
				char bits[10];
				wprintw(outputs->log,"<debug> Received character:\n");
				wprintw(outputs->log,"\tName: %s\n",tempc.name);
				wprintw(outputs->log,"\tDescription: %s\n",tempc.desc);
				wprintw(outputs->log,"\tFlags: %s|",itobstr(tempc.flags,bits));
				wprintw(outputs->log,"ATK: %d|",tempc.attack);
				wprintw(outputs->log,"DEF: %d|",tempc.defence);
				wprintw(outputs->log,"REG: %d|",tempc.regen);
				wprintw(outputs->log,"HP: %d|",tempc.health);
				wprintw(outputs->log,"Gold: %d|",tempc.gold);
				wprintw(outputs->log,"Room: %d|",tempc.room);
				wprintw(outputs->log,"DescLen: %d\n",tempc.descl);
				
				wrefresh(outputs->log);
			}
			if(getbit(5,tempc.flags)){ //Checks if character is an enemy
				wattron(outputs->log,COLOR_PAIR(2)); //Changes fancy color to red to indicate danger... oooohhh
				for(int i=0; i<enemycount; i++){ //Checks saved data and saves new data if names are the same. Log is updated with useful stuff as well
					if (!strcmp(enemies[i].name, tempc.name)){
						if(verbose){
							wprintw(outputs->log, "<debug> Found enemy %s stored. Updating...\n",tempc.name);
							wrefresh(outputs->log);
						}
						if(changetext){
							if(tempc.health<enemies[i].health){
								wprintw(outputs->log, "<enemy> %s lost %d health, remaining: %d\n",tempc.name,enemies[i].health-tempc.health, tempc.health);
								wrefresh(outputs->log);
							}
							if(tempc.health>enemies[i].health){
								wprintw(outputs->log, "<enemy> %s gained %d health, remaining: %d\n",tempc.name,tempc.health-enemies[i].health, tempc.health);
								wrefresh(outputs->log);
							}
							if((getbit(7,enemies[i].flags) != getbit(7,tempc.flags)) && getbit(7,tempc.flags)){
								wprintw(outputs->log, "<enemy> %s Came to life! Health: %d\n",tempc.name, tempc.health);
								wrefresh(outputs->log);
							}
							if((getbit(7,enemies[i].flags) != getbit(7,tempc.flags)) && !getbit(7,tempc.flags)){
								wprintw(outputs->log, "<enemy> %s died!\n",tempc.name);
								wrefresh(outputs->log);
							}
						}

						enemies[i] = tempc;
						stored = true;
						break;
					}
				}
				if(!stored){ //Packet was not found in data so it saves it in the next index and increments enemycount
					if(verbose){
						wprintw(outputs->log, "<debug> Enemy %s not stored. Storing...\n",tempc.name);
						wrefresh(outputs->log);
					}
					wprintw(outputs->log, "<enemy> %s, %s\n",tempc.name, tempc.desc);
					wrefresh(outputs->log);
					enemies[enemycount] = tempc;
					enemycount++;
					if(enemycount >= OTHER_MAX-1) enemycount--;
				}
				stored = false;
				wattroff(outputs->log,COLOR_PAIR(2));
			

			}else if(!strcmp(players[0].name, tempc.name)){ //If the packet happens to have the name of the user It is stored after outputting relevant new info
				
				if(verbose){
					wprintw(outputs->log, "<debug> Found user player %s stored. Updating...\n",tempc.name);
					wrefresh(outputs->log);
				}
				if(changetext){
					if((getbit(7,players[0].flags) != getbit(7,tempc.flags)) && !getbit(7,tempc.flags)){
						wprintw(outputs->log, "<local> You died!\n");
						wrefresh(outputs->log);
					}
					else if(tempc.health>players[0].health){
						wprintw(outputs->log, "<local> You gained %d health, remaining: %d\n",tempc.health-players[0].health, tempc.health);
						wrefresh(outputs->log);
					}
					if(tempc.health<players[0].health){
						wprintw(outputs->log, "<local> You lost %d health, remaining: %d\n",players[0].health-tempc.health, tempc.health);
						wrefresh(outputs->log);
					}
					if((getbit(7,players[0].flags) != getbit(7,tempc.flags)) && getbit(7,tempc.flags)){
						wprintw(outputs->log, "<local> You came to life! Health: %d\n", tempc.health);
						wrefresh(outputs->log);
					}
					if(players[0].gold < tempc.gold){
						wprintw(outputs->log, "<local> You got %d gold! Gold: %d\n", tempc.gold-players[0].gold,tempc.gold);
						wrefresh(outputs->log);
					}
					if(players[0].gold > tempc.gold){
						wprintw(outputs->log, "<local> You lost %d gold! Gold: %d\n", players[0].gold - tempc.gold,tempc.gold);
						wrefresh(outputs->log);
					}
				}
				

				players[0] = tempc;
				
			}else{
				wattron(outputs->log,COLOR_PAIR(3));//Changes fancy color to blue to indicate... playerness?
				for(int i=1; i<playercount; i++){ //Checks saved data and saves new data if names are the same. Log is updated with useful stuff as well
					if (!strcmp(players[i].name, tempc.name)){
						if(verbose){
							wprintw(outputs->log, "<debug> Found player %s stored. Updating...\n",tempc.name);
							wrefresh(outputs->log);
						}
						if(changetext){
							if(tempc.health<players[i].health){
								wprintw(outputs->log, "<player> %s lost %d health, remaining: %d\n",tempc.name,players[i].health-tempc.health, tempc.health);
								wrefresh(outputs->log);
							}
							if(tempc.health>players[i].health){
								wprintw(outputs->log, "<player> %s gained %d health, remaining: %d\n",tempc.name,tempc.health-players[i].health, tempc.health);
								wrefresh(outputs->log);
							}
							if((getbit(7,players[i].flags) != getbit(7,tempc.flags)) && getbit(7,tempc.flags)){
								wprintw(outputs->log, "<player> %s Came to life! Health: %d\n",tempc.name, tempc.health);
								wrefresh(outputs->log);
							}
							if((getbit(7,players[i].flags) != getbit(7,tempc.flags)) && !getbit(7,tempc.flags)){
								wprintw(outputs->log, "<player> %s died!\n",tempc.name);
								wrefresh(outputs->log);
							}
							if(players[i].room != tempc.room){	
								leftroom = true;
								int changedroom;
								for(int i = 0; i<connectioncount; i++){
									if(tempc.room == rooms[i].number){
										changedroom = i;
										break;
									}
								}
								
								wprintw(outputs->log, "<player> %s went to %s!\n",tempc.name, rooms[changedroom].name);
								wrefresh(outputs->log);
							}
						}
					
						if(!leftroom)players[i] = tempc;
						else removeplayer(i);
						leftroom = false;
						stored = true;
						break;
					}
				}
				if(!stored){
					if(verbose){//Packet was not found in data so it saves it in the next index and increments playercount
						wprintw(outputs->log, "<debug> Player %s not stored. Storing...\n",tempc.name);
						wrefresh(outputs->log);
					}
					wprintw(outputs->log, "<player> %s, %s\n",tempc.name, tempc.desc);
					wrefresh(outputs->log);
					players[playercount] = tempc;
					playercount++;
					if(playercount >= OTHER_MAX) playercount--;
				}
				stored = false;
		
				for(int i=0; i<friendcount; i++){
					if (!strcmp(friends[i].name, tempc.name)){
						friends[i] = tempc;
					}
				}

				wattroff(outputs->log,COLOR_PAIR(3));
				

			}
		}

		else if(type == 9){ //ROOM Gets a room packet and saves it to rooms[0] as the current room. Also resets counters for struct arrays so new data can be written in for each room
				    //also sets started to true
				    //This gives a limitation to seeing who is on the server as players are cleared and you can only see the players in the current room.
			struct room tempr = {0};
			lurk_room(skt,1,&tempr);
			if(verbose){
				wprintw(outputs->log, "<debug> Received room:\n",tempr.name);
				wprintw(outputs->log, "\tName: %s\n",tempr.name);
				wprintw(outputs->log, "\tDescription: %s\n",tempr.desc);
				wprintw(outputs->log, "\tNumber: %d|",tempr.number);
				wprintw(outputs->log, "DescLen: %d\n",tempr.descl);
				wrefresh(outputs->log);
			}

			if(strcmp(rooms[0].name,tempr.name)){
				if(verbose){
					wprintw(outputs->log, "<debug> New room %s recieved. Storing...\n",tempr.name);
					wrefresh(outputs->log);
				}
				wattron(outputs->log,COLOR_PAIR(1));//Green for roominess
				wprintw(outputs->log, "<room> %s: %s\n",tempr.name,tempr.desc);
				wattroff(outputs->log,COLOR_PAIR(1));
				wrefresh(outputs->log);

				rooms[0]=tempr;

				connectioncount = 1;
				playercount = 1;
				enemycount = 0;

				started = true;
			}
			
		}
		else if(type == 8){ //ACCEPT Gets accept packet and prints info if verbose or if a code 10 is acccepted, setting characcept to true
			int code = lurk_accept(skt,1,0);
			if(verbose){
				mvwprintw(outputs->log,log_y-1,0, "<debug> Server accepted %d\n", (int)code);
				wrefresh(outputs->log);
			}
			if((int)code == 10) {
				characcept = true;
				wprintw(outputs->log, "<local> Character accepted! You may have to type /start\n");
				wrefresh(outputs->log);
			}
		
		}
		else if(type == 7){//ERROR Gets error packet and prints details
			struct error tempe = {0};
			lurk_error(skt,1,&tempe);
			wprintw(outputs->log, "<server> Error %d: %s\n",tempe.code,tempe.msg);
			wrefresh(outputs->log);
			
		}
		else if(type == 1){//MESSAGE Gets message packets and prints details
			struct message tempm = {0};
			lurk_message(skt,1,&tempm);
			if(verbose){
				mvwprintw(outputs->log,log_y-1,0, "<debug> Received message:\n");
				wprintw(outputs->log, "\tLength: %d\n",tempm.length);
				wprintw(outputs->log, "\tSender: %s\n",tempm.sname);
				wprintw(outputs->log, "\tReceiver: %s\n",tempm.rname);
				wrefresh(outputs->log);
			}
			//if(!strcmp(rname,players[0].name)){
				mvwprintw(outputs->log,log_y-1,0, "<%s> %s\n", tempm.sname,tempm.msg);
			//}
			wrefresh(outputs->log);
			
		}
		//wrefresh(outputs->log);
		
	}
}

//Resizes the windows in the program nicelyish. Much better than closing and reopening the program to resize stuff
void resize_window(void *arg){
	struct receive_info* prfi = (struct receive_info*)arg;
	int test_x, test_y;
	getmaxyx(stdscr,test_y,test_x);
	if(max_x!=test_x || max_y!=test_y){
		max_x = test_x;
      		max_y = test_y;
	
		wresize(prfi->header, 2,max_x); //Resize to fit
     		wresize(prfi->log, max_y-6, max_x);
		wresize(prfi->pstatus, 3,max_x);
		wresize(prfi->inputwin, 1, max_x);

		mvwin(prfi->header, 0,0); //Move to proper position
		mvwin(prfi->log, 2, 0);
		mvwin(prfi->pstatus, max_y-4,0);
		mvwin(prfi->inputwin, max_y-1, 0);
	
			
		getmaxyx(prfi->log,log_y,log_x);
		if(log_y > 32 && log_x >=96){ //Extra window stuff
			extrawin1advance = 0;
			extrawin2advance = 0;
			wresize(prfi->log, log_y, max_x-35);
			wresize(prfi->extrawindow1, log_y/2-1, 35);
			wresize(prfi->extrawindow2, log_y/2+(log_y%2)+1,35);
			mvwin(prfi->extrawindow1, 2, max_x-35);
			mvwin(prfi->extrawindow2, log_y/2+1, max_x-35);
			extrawin = true;
		}
		else extrawin = false;
		wnoutrefresh(prfi->header);
		wnoutrefresh(prfi->log);
		wnoutrefresh(prfi->pstatus);
		wnoutrefresh(prfi->inputwin);
		wnoutrefresh(prfi->extrawindow1);
		wnoutrefresh(prfi->extrawindow2);
	}
}

//Main loop that sets up curses, sets up the connection, starts the server thread, and listens for user input along with updating the windows
int main(int argc, char ** argv){
	players = malloc(charactersize);
	enemies = malloc(charactersize);
	rooms = malloc(roomsize);
	friends = malloc(charactersize);

	char tempin[1024];
	pastcommand[0] = 0;
	//curses start
	signal(SIGINT,handle_signal);
	initscr();
	getmaxyx(stdscr,max_y,max_x);
	WINDOW *header = newwin(2,max_x,0,0);
	WINDOW *log = newwin(max_y-6, max_x, 2, 0);
	WINDOW *pstatus = newwin(3,max_x,max_y-4,0);
	WINDOW *inputwin = newwin(1, max_x, max_y-1, 0);
	getmaxyx(log,log_y,log_x);
	scrollok(log,true);
	//noecho();
	//cbreak();
	keypad(inputwin,true);
	
	
	WINDOW *extrawindow1 = newwin(log_y/2-1, 35, 2, max_x-35);
	WINDOW *extrawindow2 = newwin(log_y/2+(log_y%2)+1, 35, log_y/2+1, max_x-35);
	if(log_y > 32 && log_x >=96){
		wresize(log, log_y, max_x-35);
		extrawin = true;
	}


	start_color();
	use_default_colors();
	init_pair(1, COLOR_GREEN, -1);
	init_pair(2, COLOR_RED, -1);
	init_pair(3, COLOR_BLUE, -1);
	init_pair(4, COLOR_CYAN, -1);

	endwin();
	refresh();
	update_header(header);
	update_pstatus(pstatus);

	//Set up input info struct for input stuff
	struct input_info ii;
	ii.log = log;	
	ii.inputwin = inputwin;

	//Default Player info. Name is entered in program
	players[0].name[0] = 0;
	strcpy(players[0].name,"local");
	players[0].flags = 0;
	players[0].attack = 60;
	players[0].defence = 20;
	players[0].regen = 20;
	players[0].health = 0;
	players[0].gold = 0;
	players[0].room = 0;
	strcpy(players[0].desc,"Logged in from LurkWolf");
	players[0].descl = strlen(players[0].desc)+1;



	//Start up and connect
	mvwprintw(log,log_y-1,0,"			      _-----_\n");
	mvwprintw(log,log_y-1,0,"			     /       \\\n");
	mvwprintw(log,log_y-1,0,"			    |         |\n");
	mvwprintw(log,log_y-1,0,"	LurkWolf	    |         |\n");
	mvwprintw(log,log_y-1,0,"			     \\       /\n");
	mvwprintw(log,log_y-1,0,"			      -------\n");
	mvwprintw(log,log_y-1,0,"         _\n");
	mvwprintw(log,log_y-1,0,"      __/ V|\n");
	mvwprintw(log,log_y-1,0,"      \\ )  |\n");
	mvwprintw(log,log_y-1,0,"      /    /\n");
	mvwprintw(log,log_y-1,0,"     /    //\n");
	mvwprintw(log,log_y-1,0,"    |    //|\n");
	mvwprintw(log,log_y-1,0,"    /     ||\n");
	mvwprintw(log,log_y-1,0,"   /      ||\n");
	mvwprintw(log,log_y-1,0," (@@@@@D__|_}\n\n\n");


	mvwprintw(log,log_y-1,0, "<local> Welcome to LurkWolf Made by Alexander Romero 2018\n");//Initial Splash
	if(argc < 3){//If ip and port are not entered on the command line
		char tempip[32];
		char tempport[32];
typeconnect:
		wprintw(log, "<local> Please enter connection address followed by the port\n");
		wrefresh(log);
		strcpy(tempip,request(inputwin,"IP: ",tempin));
		strcpy(tempport,request(inputwin,"Port: ",tempin));
		strcpy(ip, tempip);
		strcpy(port, tempport);
		mvwprintw(log,log_y-1,0,"<local> Connecting to: %s %s\n", tempip, tempport);
		wrefresh(log);
		}
	else{
		strcpy(ip, argv[1]);
		strcpy(port, argv[2]);
	}
	
	//All this connection jazz
	skt = socket(AF_INET, SOCK_STREAM, 0);
	if(skt == -1){
		perror("Socket:  ");
		return 1;
	}

	struct sockaddr_in saddr;
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(atoi(port));;

	struct hostent* entry = gethostbyname(ip);
	struct in_addr **addr_list = (struct in_addr**)entry->h_addr_list;
	struct in_addr* c_addr = addr_list[0];
	char* ip_string = inet_ntoa(*c_addr);
	wprintw(log, "<local> IP address: %s\n",ip_string);
	wrefresh(log);
	saddr.sin_addr = *c_addr;

	wprintw(log, "<local> Connecting...\n");
	wrefresh(log);
	if(connect(skt, (struct sockaddr*)&saddr, sizeof(saddr))){
		wprintw(log, "<local> Failed to connect\n");
		wrefresh(log);
		connected = false;
		goto typeconnect; //Goes back to try again if it fails
	}else connected = true;

	//Set up receive_info struct for server thread
	struct receive_info rinfo;
	rinfo.skt = skt;
	rinfo.input = inputwin;
	rinfo.log = log;
	rinfo.pstatus = pstatus;
	rinfo.inputwin = inputwin;
	rinfo.header = header;
	rinfo.extrawindow1 = extrawindow1;
	rinfo.extrawindow2 = extrawindow2;
	
	//Server connection thread
	pthread_t serverthread;
	pthread_create(&serverthread, NULL, server_handler, &rinfo);
	
	endwin();
	refresh();

	for(;;){ // Main loop starts here
		/*if(connected == false){
			goto typeconnect;//Kicks you back if disconnected
		}*/
		char input[1024];
		resize_window(&rinfo); //Update stuff
		update_header(header);
		update_pstatus(pstatus);
		update_extrawin(extrawindow1, extrawindow2);
		doupdate();
		wmove(inputwin,0,1);
		wrefresh(log);
		wclear(inputwin);
		get_input(&ii,input); //Get input
		wrefresh(log);
		if(!(input[0]=='/' || input[0] == 0)){//If text is entered with no / then it is sent as a message with no recipiant(would be nice for global chat)
			int type = 1;
			char rname[32];
			int mlen = strlen(input);
			rname[0] = 0;
			if(quickmsg){
				for(int i=0; i < friendcount; i++){
					wprintw(log,"<local> Sending to: %s\n", friends[i].name);
					strcpy(rname,friends[i].name);
					write(skt, &type, 1);
					write(skt, &mlen, 2);
					write(skt, &rname, 32);
					write(skt, &players[0].name, 32);
					write(skt, &input, mlen);
				}
			}else{
				write(skt, &type, 1);
				write(skt, &mlen, 2);
				write(skt, &rname, 32);
				write(skt, &players[0].name, 32);
				write(skt, &input, mlen);
			}
		}
		else if(input[0] == 0){//If nothing is put in the input window but enter is pressed
			//Do nothing!
		}
		else{
			char* stay;
			char* token;
			char yes[64];
			token = yes;
			token = strtok_r(input," ",&stay); //Set up splitting the input into tokens for multi word inout

			//Begin lurk commands
			
			if(!strcmp(input,"/msg")){//Input recipiant and message to be sent to server
				struct message send;
				strcpy(send.sname,players[0].name);
				bool found = false;
				token = strtok_r(NULL,"",&stay);
				if(token == NULL){ //If no name was given for second token
					wprintw(log,"<local> Players Available:\n");
					for(int i = 1; i < playercount; i++){
						wprintw(log,"\t%s\n",players[i].name);
					}
					wrefresh(log);
					strcpy(send.rname,request(inputwin,"Recipient: ",input));
				}else strcpy(send.rname,token);
				strcpy(send.msg,request(inputwin,"Message: ",input));
				send.length = strlen(send.msg)+1;
				if(verbose){
					wprintw(log,"<debug> Sending message:\n");
					wprintw(log,"\tSender: %d\n",send.length);
					wprintw(log,"\tSender: %s\n",send.sname);
					wprintw(log,"\tReceiver: %s\n",send.rname);
					wprintw(log,"\tMessage: %s\n",send.msg);
					wrefresh(log);
				}
				lurk_message(skt,0,&send);
				wprintw(log, "<%s> %s\n",players[0].name, send.msg);
				wrefresh(log);
				
			}else if(!strcmp(input,"/msgl")){// Gives list of players to message and gets message to be sent to server
				struct message send;
				int numget;
				strcpy(send.sname,players[0].name);
				wprintw(log,"<local> Players Available:\n");
				for(int i = 1; i < playercount; i++){
					wprintw(log,"\t%d: %s\n",i,players[i].name);
				}
				wrefresh(log);
				numget = atoi(request(inputwin,"Recipient number: ",input));
				if(numget < 0)numget = 0;
				if(numget > playercount)numget = 0;
				strcpy(send.rname, players[numget].name);
				wprintw(log,"<local> Sending to: %s\n",send.rname);
				wrefresh(log);
				strcpy(send.msg,request(inputwin,"Message: ",input));
				send.length = strlen(send.msg)+1;
				if(verbose){
					wprintw(log,"<debug> Sending message:\n");
					wprintw(log,"\tSender: %d\n",send.length);
					wprintw(log,"\tSender: %s\n",send.sname);
					wprintw(log,"\tReceiver: %s\n",send.rname);
					wprintw(log,"\tMessage: %s\n",send.msg);
					wrefresh(log);
				}
				lurk_message(skt,0,&send);
				wprintw(log, "<%s> %s\n",players[0].name, send.msg);
				wrefresh(log);
			}
			else if(!strcmp(input,"/connect")){
				if(!connected){
					goto typeconnect;
				}
			}
			else if(!strcmp(input,"/quit") || !strcmp(input,"/q") || !strcmp(input,"/exit")){ //Nicely quits using the lurk_leave command and all the good stuff
				if(!strcmp(request(inputwin,"Really quit?(y/n): ",input),"y")){
					wprintw(log,"<local> Quitting...\n");
					wrefresh(log);
					lurk_leave(skt);
					endwin();
					close(skt);
					exit(0);		
				}
			}
			else if(!strcmp(input,"/login")){ //Shortcut for /make. just gets name and uses default info to send to server
				if(!characcept){			
					wprintw(log,"<local> Logging in...\n");
					token = strtok_r(NULL,"",&stay);
					if(token == NULL)strcpy(players[0].name, request(inputwin,"Enter Name: ",input));//If no name was given for second token
					else strcpy(players[0].name,token);

					players[0].attack = points*.6;
					players[0].defence = points*.2;
					players[0].regen = points*.2;
				
					wprintw(log,"<local> Sending...\n");
					wrefresh(log);
					if(verbose){
						wprintw(log,"<debug> Sending character:\n");
						wprintw(log,"\tName: %s\n",players[0].name);
						wprintw(log,"\tFlags: %s\n",itobstr(players[0].flags,input));
						wprintw(log,"\tAttack: %d\n",players[0].attack);
						wprintw(log,"\tDefense: %d\n",players[0].defence);
						wprintw(log,"\tRegen: %d\n",players[0].regen);
						wprintw(log,"\tHealth: %d\n",players[0].health);
						wprintw(log,"\tGold: %d\n",players[0].gold);
						wprintw(log,"\tRoom: %d\n",players[0].room);
						wprintw(log,"\tDesc Length: %d\n",players[0].descl);
						wprintw(log,"\tDescription: %s\n",players[0].desc);
						wrefresh(log);
					}
					int type = 10;
					lurk_character(skt,0,&players[0]);
				}else{
					wprintw(log,"<local> A character has already been accepted\n");
					wrefresh(log);
				}
			}
			else if(!strcmp(input,"/sendchar")){
				if(characcept){
					lurk_character(skt,0,&players[0]);
					wprintw(log,"<local> Sent character packet\n");
				}else{
					wprintw(log,"<local> A character needs to be accepted first. Try /login or /make.\n");
				}
			}
			else if(!strcmp(input,"/make")){ //Walks you through making a character packet leaving some things out like flags and the sort
				if(!characcept){			
					wprintw(log,"<local> Creating character...\n");

					wrefresh(log);
					strcpy(players[0].name, request(inputwin,"Enter Name: ",input));
				
					strcpy(players[0].desc, request(inputwin,"Enter Description: ",input));
					players[0].descl = strlen(players[0].desc) + 1;
			
					mvwprintw(pstatus,1,0,"Remaining points: %d     ", points);
					wrefresh(pstatus);
					players[0].attack = atoi(request(inputwin,"Enter Attack: ",input));
	
					mvwprintw(pstatus,1,0,"Remaining points: %d     ", points-players[0].attack);
					wrefresh(pstatus);
					players[0].defence = atoi(request(inputwin,"Enter Defense: ",input));
	
					mvwprintw(pstatus,1,0,"Remaining points: %d     ", points-players[0].attack-players[0].defence);
					wrefresh(pstatus);
					players[0].regen = atoi(request(inputwin,"Enter Regen: ",input));
				
					mvwprintw(pstatus,1,0,"                            ");
					wrefresh(pstatus);
				
					wprintw(log,"\n<local> Name: %s\n", players[0].name);
					wprintw(log,"\tDescription: %s\n", players[0].desc);
					wprintw(log,"\tAttack: %d\n", players[0].attack);
					wprintw(log,"\tDefense: %d\n", players[0].defence);
					wprintw(log,"\tRegen: %d\n\n", players[0].regen);
					wprintw(log,"<local> Is this correct?(y/n)\n");
					wrefresh(log);
					if(!strcmp(request(inputwin,"(y/n): ",input),"y")){
	
						wprintw(log,"<local> Sending...\n");
						wrefresh(log);
						if(verbose){
							wprintw(log,"<debug> Sending character:\n");
							wprintw(log,"\tName: %s\n",players[0].name);
							wprintw(log,"\tFlags: %s\n",itobstr(players[0].flags,input));
							wprintw(log,"\tAttack: %d\n",players[0].attack);
							wprintw(log,"\tDefense: %d\n",players[0].defence);
							wprintw(log,"\tRegen: %d\n",players[0].regen);
							wprintw(log,"\tHealth: %d\n",players[0].health);
							wprintw(log,"\tGold: %d\n",players[0].gold);
							wprintw(log,"\tRoom: %d\n",players[0].room);
							wprintw(log,"\tDesc Length: %d\n",players[0].descl);
							wprintw(log,"\tDescription: %s\n",players[0].desc);
							wrefresh(log);
						}
						lurk_character(skt,0,&players[0]);
					
					
					}else{
						wprintw(log,"<local> Type \"/make\" to try again\n");
						wrefresh(log);
					}
				}else{
					wprintw(log,"<local> A character has already been accepted\n");
					wrefresh(log);
				}
	
			}
			else if(!strcmp(input,"/start")){ //Sends simple start command
				if(verbose){
					wprintw(log,"<debug> Sending start command\n");
					wrefresh(log);
				}
				lurk_start(skt);
			}
			else if(!strcmp(input,"/joinbattle")){ //supposed to change flag bit for joinbattle to true and send to server. Might work depending on server. Im not holding my breath
				//Might do something... ask seth if we can later set join fight bit...
				wprintw(log,"<local> Sending...\n");
				wrefresh(log);
				int new = setbit(6,1,players[0].flags);
				players[0].descl += 1;
				players[0].flags = new;
				lurk_character(skt,0,&players[0]);
				wrefresh(pstatus);
			}
			else if(!strcmp(input,"/fight")){ //Sends simple fight command
				wprintw(log,"<local> Initiating Fight!\n");
				if(verbose){
					wprintw(log,"<debug> Sending fight command\n");
				}
				wrefresh(log);
				lurk_fight(skt);
			}
			else if(!strcmp(input,"/pvp")){ //Sends pvp command to initiate battle with another player. uses tokens to get name
				char name[32];
				token = strtok_r(NULL,"",&stay);
				if(token == NULL){//If no name was given for second token
					wprintw(log,"<local> Players Available:\n");
					for(int i = 1; i < playercount; i++){
						wprintw(log,"\t%s\n",players[i].name);
					}
					wrefresh(log);
					strcpy(name,request(inputwin,"WhoToFight: ",input));
				}else strcpy(name,token);
				wprintw(log,"<local> Picking a fight with %s\n",name);
				wrefresh(log);
				if(verbose){
					wprintw(log,"<debug> Sending pvp command:\n");
					wprintw(log,"\tTarget: %s\n",name);
					wrefresh(log);
				}
				lurk_pvpfight(skt,0,name);
			}
			else if(!strcmp(input,"/pvpl")){//Gives list of players to pvp with and accepts integer input
				int pvpnum;
				wprintw(log,"<local> Players Available:\n");
				for(int i = 1; i < playercount; i++){
					wprintw(log,"\t%d: %s\n",i,players[i].name);
				}
				wrefresh(log);
				pvpnum = atoi(request(inputwin,"Target number: ",input));
				if(pvpnum < 0)pvpnum = 0;
				if(pvpnum > playercount)pvpnum = 0;
				wprintw(log,"<local> Picking a fight with %s\n",players[pvpnum].name);
				wrefresh(log);
				if(verbose){
					wprintw(log,"<debug> Sending pvp command:\n");
					wprintw(log,"\tTarget: %s\n",players[pvpnum].name);
					wrefresh(log);
				}
				lurk_pvpfight(skt,0,players[pvpnum].name);
			}
			else if(!strcmp(input,"/loot")){//Sends loot command of player or enemy you want to loot
				char lname[32];
				token = strtok_r(NULL,"",&stay);
				if(token == NULL){//If no name was given for second token
					wprintw(log,"<local> Available to Loot:\n");
					for(int i = 1; i < playercount; i++){
						if(!getbit(7,players[i].flags)){
							wprintw(log,"\tP: %s\n",players[i].name);
						}
					}
					for(int i = 0; i < enemycount; i++){
						if(!getbit(7,enemies[i].flags)){
							wprintw(log,"\tE: %s\n",enemies[i].name);
						}
					}
					wrefresh(log);
					strcpy(lname,request(inputwin,"WhoToLoot: ",input));
					
				}else strcpy(lname,token);
				wprintw(log,"<local> Looting: %s\n", lname);
				wrefresh(log);
				if(verbose){
					wprintw(log,"<debug> Sending loot command:\n");
					wprintw(log,"\tTarget: %s\n",name);
					wrefresh(log);
				}
				lurk_loot(skt,0,lname);
			}
			else if(!strcmp(input,"/lootlp")){ //Gives list of players to loot and gets integer input
				char lnumber;
				wprintw(log,"<local> Available to Loot:\n");
				for(int i = 1; i < playercount; i++){
					if(!getbit(7,players[i].flags)){
						wprintw(log,"\t%d: %s\n",i+1,players[i].name);
					}
				}
				wrefresh(log);
				lnumber = atoi(request(inputwin,"WhoToLoot: ",input))-1;
				if(lnumber < 0)lnumber = 0;
				if(lnumber > playercount)lnumber = 0;
				wprintw(log,"<local> Looting: %s\n", players[lnumber].name);
				wrefresh(log);
				if(verbose){
					wprintw(log,"<debug> Sending loot command:\n");
					wprintw(log,"\tTarget: %s\n",players[lnumber].name);
					wrefresh(log);
				}
				lurk_loot(skt,0,players[lnumber].name);
			}
			else if(!strcmp(input,"/lootle")){ //Gives list of enemies to loot and gets integer input
				char lnumber;
				wprintw(log,"<local> Available to Loot:\n");
				for(int i = 0; i < enemycount; i++){
					if(!getbit(7,enemies[i].flags) && enemies[i].gold != 0){
						wprintw(log,"\t%d: %s\n",i+1,enemies[i].name);
					}
				}
				wrefresh(log);
				lnumber = atoi(request(inputwin,"WhoToLoot: ",input))-1;
				if(lnumber < 0)lnumber = 0;
				if(lnumber > enemycount)lnumber = 0;
				wprintw(log,"<local> Looting: %s\n", enemies[lnumber].name);
				wrefresh(log);
				if(verbose){
					wprintw(log,"<debug> Sending loot command:\n");
					wprintw(log,"\tTarget: %s\n",enemies[lnumber].name);
					wrefresh(log);
				}
				lurk_loot(skt,0,enemies[lnumber].name);
			}
			else if(!strcmp(input,"/lootall")){
				for(int i = 0; i < enemycount; i++){
					if(!getbit(7,enemies[i].flags) && enemies[i].gold != 0){
						lurk_loot(skt,0,enemies[i].name);
					}
				}
			}
			else if(!strcmp(input,"/goto")){ //Gets room name and sends command to go to that room
				char name[32];
				int roomnum;
				bool found;
				token = strtok_r(NULL,"",&stay);
				if(token == NULL){//If no name was given for second token
					wprintw(log,"<local> Connections Available:\n");
					for(int i = 1; i < connectioncount; i++){
						wprintw(log,"\t%s\n",rooms[i].name);
					}
					wrefresh(log);
					strcpy(name,request(inputwin,"Room name: ",input));
				}else strcpy(name,token);

				for(int i = 0; i < connectioncount; i++){
					if(!strcmp(name,rooms[i].name)){
						roomnum = rooms[i].number;
						found = true;
						break;
					}
				}
				if(found){
					wprintw(log,"<local> Going to %s\n",name);
					wrefresh(log);
					if(verbose){
						wprintw(log,"<debug> Sending changeroom command:\n");
						wprintw(log,"\tTarget: %d(%s)\n",roomnum,name);
						wrefresh(log);
					}
					lastroom = players[0].room;
					lurk_changeroom(skt,0,roomnum);
				}
				else{
					wprintw(log,"<local> No connection called %s\n",name);
					wrefresh(log);
				}
			}
			else if(!strcmp(input,"/gotonum")){ //Gets room number to go to
				int roomnum;
				token = strtok_r(NULL,"",&stay);
				if(token == NULL){
					wprintw(log,"<local> Connections Available:\n");
					for(int i = 1; i < connectioncount; i++){
						wprintw(log,"\t%d %s\n",rooms[i].number,rooms[i].name);
					}
					wrefresh(log);
					roomnum =  atoi(request(inputwin,"Room number: ",input));
				} else roomnum = atoi(token);
				wprintw(log,"<local> Going to room %d\n", roomnum);
				wrefresh(log);
				if(verbose){
					wprintw(log,"<debug> Sending changeroom command:\n");
					wprintw(log,"\tTarget: %d(%s)\n",roomnum,name);
					wrefresh(log);
				}
				lastroom = players[0].room;
				lurk_changeroom(skt,0,roomnum);
			}
			else if(!strcmp(input,"/gotol")){ //Gives list of rooms to goto and accepts integer input
				int roomnum;
				wprintw(log,"<local> Connections Available:\n");
				for(int i = 1; i < connectioncount; i++){
					wprintw(log,"\t%d: %s\n",i,rooms[i].name);
				}
				
				wrefresh(log);
				roomnum =  atoi(request(inputwin,"Room number: ",input));
				if(roomnum < 0)roomnum = 0;
				if(roomnum > connectioncount)roomnum = 0;
				wprintw(log,"<local> Going to %s\n", rooms[roomnum].name);
				wrefresh(log);
				if(verbose){
					wprintw(log,"<debug> Sending changeroom command:\n");
					wprintw(log,"\tTarget: %d(%s)\n",rooms[roomnum].number,rooms[roomnum].name);
					wrefresh(log);
				}
				lastroom = players[0].room;
				lurk_changeroom(skt,0,rooms[roomnum].number);
			}
			else if(!strcmp(input,"/goback")){ //Sends room number of the room client was last in
				if(verbose){
					wprintw(log,"<debug> Sending changeroom command:\n");
					wprintw(log,"\tTarget: %d\n",lastroom);
					wrefresh(log);
				}
				lurk_changeroom(skt,0,lastroom);
				lastroom = players[0].room;
			
			}
			// End lurk lurk commands

			// Start Client commands
			else if(!strcmp(input,"/verbose")){ //Turns on verbose output. May be half useless if there is enough stuff thrown at you and you cant scroll thorugh it
				verbose = !verbose;
				if(verbose)wprintw(log,"<local> Verbose mode enabled\n");
				else wprintw(log,"<local> Verbose mode disabled\n");
				wrefresh(log);

			}
			else if(!strcmp(input,"/win1player")){ //Changes win1 to player view
				extrawin1mode = 0;
				wprintw(log,"<local> Changing window 1 to player\n");
				wrefresh(log);
			}
			else if(!strcmp(input,"/win1enemy")){ //Changes win1 to enemy view
				extrawin1mode = 1;
				wprintw(log,"<local> Changing window 1 to enemy\n");
				wrefresh(log);
			}
			else if(!strcmp(input,"/win1room")){ //Changes win1 to room/connection view
				extrawin1mode = 2;
				wprintw(log,"<local> Changing window 1 to room\n");
				wrefresh(log);
			}
			else if(!strcmp(input,"/win1friend")){ //Changes win1 to room/connection view
				extrawin1mode = 3;
				wprintw(log,"<local> Changing window 1 to friend\n");
				wrefresh(log);
			}


			else if(!strcmp(input,"/win2player")){ //Changes win2 to player view
				extrawin2mode = 0;
				wprintw(log,"<local> Changing window 2 to player\n");
				wrefresh(log);
			}
			else if(!strcmp(input,"/win2enemy")){ //Changes win2 to enemy view
				extrawin2mode = 1;
				wprintw(log,"<local> Changing window 2 to enemy\n");
				wrefresh(log);
			}
			else if(!strcmp(input,"/win2room")){ //Changes win2 to room/connection view
				extrawin2mode = 2;
				wprintw(log,"<local> Changing window 2 to room\n");
				wrefresh(log);
			}
			else if(!strcmp(input,"/win2friend")){ //Changes win1 to room/connection view
				extrawin2mode = 3;
				wprintw(log,"<local> Changing window 2 to friend\n");
				wrefresh(log);
			}

			
			else if(!strcmp(token,"/lookat")){ //Extensive command that allows you to look at players, enemies, and connections/room as a list or singular details
				char name[32];
				char type[32];
				bool found = false;
				
				token = strtok_r(NULL," ",&stay);
				if(token == NULL){
					wprintw(log,"<local> Lookat types:\n");
					wprintw(log,"\tplayer\n");
					wprintw(log,"\tenemy\n");
					wprintw(log,"\troom\n");
					wprintw(log,"\tplayers\n");
					wprintw(log,"\tenemies\n");
					wprintw(log,"\trooms\n");
					wprintw(log,"\tplayerl\n");
					wprintw(log,"\tenemyl\n");
					wprintw(log,"\trooml\n");
					wrefresh(log);
					
					strcpy(type,request(inputwin,"Lookat type: ",input));
				} else strcpy(type,token);
				
				if(!strcmp(type,"players")){
					wprintw(log,"<local> Players:\n");
					for(int i = 0; i < playercount; i++){
						wprintw(log,"\t%s\n",players[i].name);
					}
					wrefresh(log);
					found = true;
				}
				else if(!strcmp(type,"enemies")){
					wprintw(log,"<local> Enemies:\n");
					for(int i = 0; i < enemycount; i++){
						wprintw(log,"\t%s\n",enemies[i].name);
					}
					wrefresh(log);
					found = true;
				}
				else if(!strcmp(type,"rooms")){
					wprintw(log,"<local> Rooms:\n");
					for(int i = 0; i < connectioncount; i++){
						wprintw(log,"\t%s\n",rooms[i].name);
					}
					wrefresh(log);
					found = true;
				}
				else if(!strcmp(type,"playerl")){
					int looknum;
					wprintw(log,"<local> Players available:\n");
					for(int i = 0; i < playercount; i++){
						wprintw(log,"\t%d: %s\n",i+1,players[i].name);
						wrefresh(log);
					}
					looknum = atoi(request(inputwin,"Lookat number: ",input))-1;
					if(looknum < 0)looknum = 0;
					if(looknum > playercount)looknum = 0;
					mvwprintw(log,log_y-1,0,"<local> Player: %s\n", players[looknum].name);
					wprintw(log,"\tDescription: %s\n", players[looknum].desc);
					wprintw(log,"\tStatus: ");
					if(getbit(7,players[looknum].flags)) wprintw(log,"Alive, ");
					else wprintw(log,"Dead, ");
					if(getbit(4,players[looknum].flags))  wprintw(log,"Active\n");
					else wprintw(log,"Inactive\n");
					wprintw(log,"\tHP: %d\n", players[looknum].health);
					wprintw(log,"\tATK: %d\n", players[looknum].attack);
					wprintw(log,"\tDEF: %d\n", players[looknum].defence);
					wprintw(log,"\tREG: %d\n", players[looknum].regen);
					wprintw(log,"\tGOLD: %d\n", players[looknum].gold);
					if(verbose){
						wprintw(log,"\tDesc Length: %d\n", players[looknum].descl);
						wprintw(log, "\tFlags: %s\n", itobstr(players[looknum].flags,input));
					}
					found = true;
					
				}
				else if(!strcmp(type,"enemyl")){
					int looknum;
					if(enemycount!=0){
						wprintw(log,"<local> Enemies available:\n");
						for(int i = 0; i < enemycount; i++){
							wprintw(log,"\t%d: %s\n",i+1,enemies[i].name);
							wrefresh(log);
						}
						looknum = atoi(request(inputwin,"Lookat number: ",input))-1;
						if(looknum < 0)looknum = 0;
						if(looknum > enemycount)looknum = 0;
						mvwprintw(log,log_y-1,0,"<local> Enemy: %s\n", enemies[looknum].name);
						wprintw(log,"\tDescription: %s\n", enemies[looknum].desc);
						wprintw(log,"\tStatus: ");
						if(getbit(7,enemies[looknum].flags)) wprintw(log,"Alive\n");
						else wprintw(log,"Dead\n");
						wprintw(log,"\tHP: %d\n", enemies[looknum].health);
						wprintw(log,"\tATK: %d\n", enemies[looknum].attack);
						wprintw(log,"\tDEF: %d\n", enemies[looknum].defence);
						wprintw(log,"\tREG: %d\n", enemies[looknum].regen);
						wprintw(log,"\tGOLD: %d\n", enemies[looknum].gold);
						if(verbose){
							wprintw(log,"\tDesc Length: %d\n", enemies[looknum].descl);
							wprintw(log, "\tFlags: %s\n", itobstr(enemies[looknum].flags,input));
						}
					}else{
						wprintw(log,"<local> No enemies available\n");
					}
					found = true;
					
				}
				else if(!strcmp(type,"rooml")){
					int looknum;
					wprintw(log,"<local> Rooms available:\n");
					for(int i = 0; i < connectioncount; i++){
						wprintw(log,"\t%d: %s\n",i+1,rooms[i].name);
						wrefresh(log);
					}
					looknum = atoi(request(inputwin,"Lookat number: ",input))-1;
					if(looknum < 0)looknum = 0;
					if(looknum > connectioncount)looknum = 0;
					mvwprintw(log,log_y-1,0,"<local> Room: %s\n", rooms[looknum].name);
					wprintw(log,"\tNumber: %d\n", rooms[looknum].number);
					wprintw(log,"\tDescription: %s\n", rooms[looknum].desc);
					if(verbose){
						wprintw(log,"\tDesc Length: %d\n", rooms[looknum].descl);
					}
					found = true;
					
				}
				
				else{

					token = strtok_r(NULL,"",&stay);
					if(token == NULL){
						if(!strcmp(type,"player")){
							wprintw(log,"<local> Players To lookat:\n");
							for(int i = 0; i < playercount; i++){
								wprintw(log,"\t%s\n",players[i].name);
							}
							wrefresh(log);
						}
						else if(!strcmp(type,"enemy")){
							wprintw(log,"<local> Enemies To lookat:\n");
							for(int i = 0; i < enemycount; i++){
								wprintw(log,"\t%s\n",enemies[i].name);
							}
							wrefresh(log);
						}
						else if(!strcmp(type,"room")){
							wprintw(log,"<local> Rooms To lookat:\n");
							for(int i = 0; i < connectioncount; i++){
								wprintw(log,"\t%s\n",rooms[i].name);
							}
							wrefresh(log);
						}
						strcpy(name,request(inputwin,"Lookat name: ",input));
						
					} else strcpy(name,token);
				
					if(!strcmp(type,"player")){
						for(int i = 0; i < playercount; i++){
							if(!strcmp(name,players[i].name)){
								mvwprintw(log,log_y-1,0,"<local> Player: %s\n", players[i].name);
								wprintw(log,"\tDescription: %s\n", players[i].desc);
								wprintw(log,"\tStatus: ");
								if(getbit(7,players[i].flags)) wprintw(log,"Alive, ");
								else wprintw(log,"Dead, ");
								if(getbit(4,players[i].flags))  wprintw(log,"Active\n");
								else wprintw(log,"Inactive\n");
								wprintw(log,"\tHP: %d\n", players[i].health);
								wprintw(log,"\tATK: %d\n", players[i].attack);
								wprintw(log,"\tDEF: %d\n", players[i].defence);
								wprintw(log,"\tREG: %d\n", players[i].regen);
								wprintw(log,"\tGOLD: %d\n", players[i].gold);
								if(verbose){
									wprintw(log,"\tDesc Length: %d\n", players[i].descl);
									wprintw(log, "\tFlags: %s\n", itobstr(players[i].flags, input));
								}
								found = true;
								break;
							}
						}
						if(!found) mvwprintw(log,log_y-1,0,"<local> Couldn't find Player: %s\n", name);
					}
					else if(!strcmp(type,"enemy")){
						for(int i = 0; i < enemycount; i++){
							if(!strcmp(name,enemies[i].name)){
								mvwprintw(log,log_y-1,0,"<local> Enemy: %s\n", enemies[i].name);
								wprintw(log,"\tDescription: %s\n", enemies[i].desc);
								wprintw(log,"\tStatus: ");
								if(getbit(7,enemies[i].flags)) wprintw(log,"Alive\n");
								else wprintw(log,"Dead\n");
								wprintw(log,"\tHP: %d\n", enemies[i].health);
								wprintw(log,"\tATK: %d\n", enemies[i].attack);
								wprintw(log,"\tDEF: %d\n", enemies[i].defence);
								wprintw(log,"\tREG: %d\n", enemies[i].regen);
								wprintw(log,"\tGOLD: %d\n", enemies[i].gold);
								if(verbose){
									wprintw(log,"\tDesc Length: %d\n", enemies[i].descl);
									wprintw(log, "\tFlags: %s\n", itobstr(enemies[i].flags, input));
								}
								found = true;
								break;
							}
						}
						if(!found) mvwprintw(log,log_y-1,0,"<local> Couldn't find Enemy: %s\n", name);
					}
					else if(!strcmp(type,"room")){
						for(int i = 0; i < connectioncount; i++){
							if(!strcmp(name,rooms[i].name)){
								mvwprintw(log,log_y-1,0,"<local> Room: %s\n", rooms[i].name);
								wprintw(log,"\tNumber: %d\n", rooms[i].number);
								wprintw(log,"\tDescription: %s\n", rooms[i].desc);
								if(verbose){
									wprintw(log,"\tDesc Length: %d\n", rooms[i].descl);
								}
								found = true;
								break;
							}
						}
						if(!found) mvwprintw(log,log_y-1,0,"<local> Couldn't find Room: %s\n", name);
					}
					else{
						mvwprintw(log,log_y-1,0,"<local> Please use this format: /lookat [player,enemy,room] [name]\n");
					}
					wrefresh(log);
				}
			}
			else if(!strcmp(input,"/look")){ //Outputs a little overview of the current situation
				mvwprintw(log,log_y-1,0,"<local> You are in %s: %s\n", rooms[0].name, rooms[0].desc);
				wprintw(log, "\tThere are %d player(s), %d enemies and %d room connection(s) available\n", playercount-1, enemycount, connectioncount-1);
				if(playercount == 1){
					wprintw(log, "\tSeems a little lonely don't it?\n");
				}
				wrefresh(log);
			}
			else if(!strcmp(input,"/addfriend")){
				bool found = false;
				char name[32];
				token = strtok_r(NULL,"",&stay);
				if(token == NULL){ //If no name was given for second token
					wprintw(log,"<local> Players Available:\n");
					for(int i = 1; i < playercount; i++){
						wprintw(log,"\t%s\n",players[i].name);
					}
					wrefresh(log);
					strcpy(name,request(inputwin,"Whotofriend: ",input));
				}else strcpy(name,token);
				for(int i = 0; i < playercount; i++){
					if(!strcmp(name,players[i].name)){
						friends[friendcount] = players[i];
						found = true;
						break;
					}
				}
				if(found){
					friendcount++;
					wprintw(log,"<local> %s has been added to friend list\n",name);
					wrefresh(log);
				}
				else{
					wprintw(log,"<local> Couldn't find Player: %s\n",name);
					wrefresh(log);				
				}
				
			}
			else if(!strcmp(input,"/addfriendl")){
				int numget;
				wprintw(log,"<local> Players Available:\n");
				for(int i = 1; i < playercount; i++){
					wprintw(log,"\t%d: %s\n",i,players[i].name);
				}
				wrefresh(log);
				numget = atoi(request(inputwin,"Player number: ",input));
				if(numget < 0)numget = 0;
				if(numget > playercount)numget = 0;
				friends[friendcount] = players[numget];
				friendcount++;
				wprintw(log,"<local> %s has been added to friend list\n",players[numget].name);
				wrefresh(log);
			}
			else if(!strcmp(input,"/popfriend")){
				if(friendcount>0){				
					wprintw(log,"<local> %s has been removed from friend list\n",friends[friendcount-1].name);
					friendcount--;
				}else{
					wprintw(log,"<local> You have no friends\n",friends[friendcount-1].name);
				}
				wrefresh(log);
			}
			else if(!strcmp(input,"/clrfriend")){
				wprintw(log,"<local> Friend list cleared\n");
				wrefresh(log);
				friendcount = 0;
			}
			else if(!strcmp(input,"/friendmsg")){
				quickmsg = !quickmsg;
				if(quickmsg)wprintw(log,"<local> Quick message to friends enabled\n");
				else wprintw(log,"<local> Quick message to friends disabled\n");
				wrefresh(log);
			}
			else if(!strcmp(input,"/win1detail")){ //Toggles detail
				extrawin1detail = !extrawin1detail;
				if(extrawin1detail)wprintw(log,"<local> Extra window 1 detail enabled\n");
				else wprintw(log,"<local> Extra window 1 detail disabled\n");
				wrefresh(log);
			}
			else if(!strcmp(input,"/win2detail")){ //Toggles detail
				extrawin2detail = !extrawin2detail;
				if(extrawin2detail)wprintw(log,"<local> Extra window 2 detail enabled\n");
				else wprintw(log,"<local> Extra window 2 detail disabled\n");
				wrefresh(log);
			} 
			else if(!strcmp(input,"/win1activep")){ //Toggles active players
				showinactive1 = !showinactive1;
				if(showinactive1)wprintw(log,"<local> Extra window 1 active players disabled\n");
				else wprintw(log,"<local> Extra window 1 active players enabled\n");
				wrefresh(log);
			}
			else if(!strcmp(input,"/win2activep")){ //Toggles active players
				showinactive2 = !showinactive2;
				if(showinactive2)wprintw(log,"<local> Extra window 2 active players disabled\n");
				else wprintw(log,"<local> Extra window 2 active players enabled\n");
				wrefresh(log);
			}
			else if(!strcmp(input,"/win1down")){ //Scrolls windows
				int tempx,tempy;
				getmaxyx(extrawindow1,tempy,tempx);
				if(extrawin1detail)extrawin1advance += tempy/2;
				else extrawin1advance += tempy;
				if(extrawin1advance > OTHER_MAX) extrawin1advance = 0;
				wprintw(log,"<local> Extra window 1 is advanced by %d\n", extrawin1advance);
				wrefresh(log);
			}
			else if(!strcmp(input,"/win2down")){ //Scrolls windows
				int tempx,tempy;
				getmaxyx(extrawindow2,tempy,tempx);
				if(extrawin2detail)extrawin2advance += tempy/2;
				else extrawin2advance += tempy;
				if(extrawin2advance > OTHER_MAX) extrawin2advance = 0;
				wprintw(log,"<local> Extra window 2 is advanced by %d\n", extrawin2advance);
				wrefresh(log);
			}
			else if(!strcmp(input,"/win1up")){ //Scrolls windows
				int tempx,tempy;
				getmaxyx(extrawindow1,tempy,tempx);
				if(extrawin1detail)extrawin1advance -= tempy/2;
				else extrawin1advance -= tempy;
				if(extrawin1advance < 0) extrawin1advance = 0;
				wprintw(log,"<local> Extra window 1 is advanced by %d\n", extrawin1advance);
				wrefresh(log);
			}
			else if(!strcmp(input,"/win2up")){ //Scrolls windows 
				int tempx,tempy;
				getmaxyx(extrawindow2,tempy,tempx);
				if(extrawin1detail)extrawin2advance -= tempy/2;
				else extrawin2advance -= tempy;
				if(extrawin2advance < 0) extrawin2advance = 0;
				wprintw(log,"<local> Extra window 2 is advanced by %d\n", extrawin2advance);
				wrefresh(log);
			}
			else if(!strcmp(input,"/win1reset")){ //Resets window scroll
				extrawin1advance = 0;
				wprintw(log,"<local> Extra window 1 is advanced by %d\n", extrawin2advance);
				wrefresh(log);
			}
			else if(!strcmp(input,"/win2reset")){ //Resets window scroll
				extrawin2advance = 0;
				wprintw(log,"<local> Extra window 2 is advanced by %d\n", extrawin2advance);
				wrefresh(log);
			}
			else if(!strcmp(input,"/charmsg")){
				changetext = !changetext;
				if(changetext)wprintw(log,"<local> Character differences enabled\n");
				else wprintw(log,"<local> Character differences disabled\n");
				wrefresh(log);
			}
			else if(!strcmp(input,"/itobstr")){ //Test deal for converting an integer to bit string
				int a = atoi(request(inputwin,"Number: ",input));
				wprintw(log,"<local> %d translates to %s\n", a, itobstr(a,input));
				wrefresh(log);
			}
			
			else if(!strcmp(input,"/moo")){
				token = strtok_r(NULL,"",&stay);
				if(token == NULL){
					wprintw(log,"<local>       ^-^    Moo \n");
					wprintw(log,"\t    C/| |\\D           \n");
					wprintw(log,"\t     (.._)------^^-   \n");
					wprintw(log,"\t       \\ @@   @@ @ \\  \n");
					wprintw(log,"\t        |  _@@@_(  |\\ \n");
					wprintw(log,"\t        \\ |    YY\\ | *\n");
					wprintw(log,"\t         W        W   \n");
				}else{
					struct message tempmoo;
					strcpy(tempmoo.sname,players[0].name);
					strcpy(tempmoo.rname,token);
					strcpy(tempmoo.msg,"\n");
					strcat(tempmoo.msg,"\t      ^-^    Moo \n");
					strcat(tempmoo.msg,"\t    C/| |\\D           \n");
					strcat(tempmoo.msg,"\t     (.._)------^^-   \n");
					strcat(tempmoo.msg,"\t       \\ @@   @@ @ \\  \n");
					strcat(tempmoo.msg,"\t        |  _@@@_(  |\\ \n");
					strcat(tempmoo.msg,"\t        \\ |    YY\\ | *\n");
					strcat(tempmoo.msg,"\t         W        W   \n");
					tempmoo.length = strlen(tempmoo.msg)+1;
					lurk_message(skt,0,&tempmoo);
				}		
			}
			else if(!strcmp(input,"/clear")){
				for(int i = 0; i<log_y; i++){
					wprintw(log,"\n");
				}
			}			
			else if(!strcmp(input,"/help")){ //Prints wall of text
				wprintw(log,"<local> List of Wolflurk sever communication commands:\n");
				wprintw(log,"\t/quit: Nicely closes out of the client with confirmation\n");
				wprintw(log,"\t/make: Has you input a player character to be sent\n");
				wprintw(log,"\t/login: Shorter version of make that uses default values other than name which you input\n");
				wprintw(log,"\t/start: Tells server client wants to start game\n");
				wprintw(log,"\t/msg: Lets you send a message to a player(entering text with no '/' will do a msg command with an empty recipient name)\n");
				wprintw(log,"\t/msgl: Similar to msg but gives you a numbered list of players so you can enter a number instead of an annoying username\n");
				wprintw(log,"\t/joinbattle: Supposed to change the current player character to join battles server withholding. Not tested fully.\n");
				wprintw(log,"\t/fight: Sends a fight command to server. If anything happens it should tell you\n");
				wprintw(log,"\t/pvp: Lets you enter a player name to fight\n");
				wprintw(log,"\t/pvpl: Similar to pvp but gives you a numbered list of players so you can enter a number instead of an annoying username\n");
				wprintw(log,"\t/loot: Lets you loot a character whose name you enter\n");
				wprintw(log,"\t/lootlp: Similar to loot but gives you a numbered list of players so you can enter a number instead of an annoying username\n");
				wprintw(log,"\t/lootle: Similar to loot but gives you a numbered list of enemies so you can enter a number instead of an annoying name\n");
				wprintw(log,"\t/goto: Lets you go to a connection available to you in the room\n");
				wprintw(log,"\t/gotonum: Same as goto except you use the ID number for the room you want to go to.\n");
				wprintw(log,"\t/gotol: similar to goto but gives you a numbered list of rooms so you don't even have to use gotonum\n");
				wprintw(log,"\t/goback: similar to goto but instantly takes you back to the previous room as long as the connection exists\n");
				wprintw(log,"\tType /help2 for client commands\n");
			}
			else if(!strcmp(input,"/help2")){ //More text
				wprintw(log,"\n<local> List of Wolflurk client commands:\n");
				wprintw(log,"\t/look: Tells you your current room and how many players, enemies, and connections are available\n");
				wprintw(log,"\t/lookat: Gets you details about players, enemies, and rooms or a list of each\n");
				wprintw(log,"\t/win1player: Change window to player view(substitute 1 for 2 for the other window)\n");
				wprintw(log,"\t/win1enemy: Change window to enemy view(substitute 1 for 2 for the other window)\n");
				wprintw(log,"\t/win1room: Change window to room/connection view(substitute 1 for 2 for the other window)\n");
				wprintw(log,"\t/win1friend: Change window to friend view(substitute 1 for 2 for the other window)\n");
				wprintw(log,"\t/win1detail: Toggles window to show details(substitute 1 for 2 for the other window)\n");
				wprintw(log,"\t/win1activep: Toggle window to show active players only(Those who are not dead with health over 0)(substitute 1 for 2 for the other window)\n");
				wprintw(log,"\t/win1down: Scroll list down(substitute 1 for 2 for the other window)\n");
				wprintw(log,"\t/win1up: Scroll list up(substitute 1 for 2 for the other window)\n");
				wprintw(log,"\t/win1reset: Resets scroll to 0(substitute 1 for 2 for the other window)\n");
				wprintw(log,"\t/addfriend: Lets you enter a name of an available player to add to friend list\n");
				wprintw(log,"\t/addfriendl: Lets you chose a name from a list of players to add as friend\n");
				wprintw(log,"\t/popfriend: Removes last added friend\n");
				wprintw(log,"\t/clrfriend: Removes all friends\n");
				wprintw(log,"\t/friendmsg: Enables quick message for ease of messaging\n");
				wprintw(log,"\t/charmsg: Changes if you see changes on characters\n");
				wprintw(log,"\t/verbose: Enables verboseness good for debugging and the such\n");
				wprintw(log,"\t/itobstr: For testing only, give it an integer and it will return a binary conversion(Might have a memory leak)\n");
				wprintw(log,"\t/moo: moo\n");
				wprintw(log,"\tType /help for server commands\n");
			}
			else{
				wprintw(log,"<local> Couldn't find command: %s\n", input); // If input with / at the start is not found
				wrefresh(log);
			}
			usleep(100000);
	
		}
	}

	return 0;
}
