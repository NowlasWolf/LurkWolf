#ifndef  LURK_H
#define LURK_H

#include<stdbool.h>
#include<unistd.h>
#include<stdlib.h>


struct character {
	char name[32];
	int flags;
	int attack;
	int defence;
	int regen;
	signed int health;
	int gold;
	int room;
	int descl;
	char desc[1024*64];
};

struct room {
	int number;
	char name[32];
	int descl;
	char desc[1024*64];
};

struct message{
	int length;
	char rname[32];
	char sname[32];
	char msg[1024*64];
};

struct game{
	int ipoints;
	int limit;
	int descl;
	char desc[1024*64];
};

struct error{
	int code;
	int msgl;
	char msg[1024*64];
};

//char readstring[1024*1024];


bool getbit(int bitpos, int data);

int setbit(int bitpos, bool state ,int data);

char* itobstr(int data, char* out);

void lurk_message(int skt, int mode, void* pkt);

int lurk_changeroom(int skt, int mode, int roomnum);

void lurk_fight(int skt);

char* lurk_pvpfight(int skt, int mode, char name[]);

char* lurk_loot(int skt, int mode, char name[]);

void lurk_start(int skt);

void lurk_error(int skt, int mode, void* pkt);

int lurk_accept(int skt, int mode,int code);

void lurk_room(int skt, int mode, void* pkt);

void lurk_character(int skt, int mode, void* pkt);

void lurk_game(int skt, int mode, void* pkt);

void lurk_leave(int skt);

void lurk_connection(int skt, int mode, void* pkt);


#endif
