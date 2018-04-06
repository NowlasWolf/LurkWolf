/*
 *This should contain the functions neccessary for comunicating back and forth with a lurk client and/or server
 *Some other functions that are quite helpful are included as well	
 *Created by Alexander Romero 2018
 */
#include<stdio.h>
#include "lurk.h"


bool getbit(int bitpos, int data){
	data = data;
	int test = 1 << bitpos;
	bool final = test & data;
	return final;

}

int setbit(int bitpos, bool state ,int data){
	bool test = getbit(bitpos,data);
	if(test == state)return data;
	data ^= (1 << bitpos);
	return data;

}

char* itobstr(int data, char* out){
	for(int i = 0; i < 8; i++){
		bool what = getbit(i,data);
		if(what)out[7-i]='1';
		else out[7-i]='0';
	}
	out[8] = 0;
	return out;
}

// Lurk Protocol //

//TYPE 1 MESSAGE
void lurk_message(int skt, int mode, void* pkt){
	if(mode == 0){
		struct message* outmessage = (struct message*)pkt;
		int type = 1;
		write(skt, &type, 1);
		write(skt, &outmessage->length, 2);
		write(skt, &outmessage->rname, 32);
		write(skt, &outmessage->sname, 32);
		write(skt, &outmessage->msg, outmessage->length);
	}
	else if(mode == 1){
		struct message* inmessage = (struct message*)pkt;
		read(skt, &inmessage->length, 2);
		read(skt, &inmessage->rname, 32);
		read(skt, &inmessage->sname, 32);
		read(skt, &inmessage->msg, inmessage->length);
	}
}

//TYPE 2 CHANGEROOM
int lurk_changeroom(int skt, int mode, int roomnum){
	if(mode == 0){
		int type = 2;
		write(skt,&type,1);
		write(skt,&roomnum,2);
	}
	else if(mode == 1){
		read(skt,&roomnum,2);
		return roomnum;
	}
}

//TYPE 3 FIGHT ONLY USED BY CLIENT
void lurk_fight(int skt){
	int type = 3;
	write(skt,&type,1);
}

//TYPE 4 PVPFIGHT
char* lurk_pvpfight(int skt, int mode, char name[]){
	if(mode == 0){
		int type = 4;
		write(skt,&type,1);
		write(skt,name,32);
	}
	else if(mode == 1){
		read(skt,name,32);
		return name;
	}
}

//TYPE 5 LOOT
char* lurk_loot(int skt, int mode, char name[]){
	if(mode == 0){
		int type = 5;
		write(skt,&type,1);
		write(skt,name,32);
	}
	else if(mode == 1){
		write(skt,name,32);
		return name;
	}
}

//TYPE 6 START ONLY USED BY CLIENT
void lurk_start(int skt){
	int type = 6;
	write(skt,&type,1);
}

//TYPE 7 ERROR
void lurk_error(int skt, int mode, void* pkt){
	if(mode == 0){
		struct error* outerror = (struct error*)pkt;
		int type = 7;
		write(skt, &type, 1);
		write(skt, &outerror->code, 1);
		write(skt, &outerror->msgl, 2);
		write(skt, &outerror->msg, outerror->msgl);
	}
	else if(mode == 1){
		struct error* inerror = (struct error*)pkt;
		read(skt, &inerror->code, 1);
		read(skt, &inerror->msgl, 2);
		read(skt, &inerror->msg, inerror->msgl);
	}
}

//TYPE 8 ACCEPT
int lurk_accept(int skt, int mode,int code){
	if(mode==0){
		int type = 8;
		write(skt,&type,1);
		write(skt,&code,1);
	}
	else if(mode==1){
		read(skt,&code,1);
		return code;
	}
}

//TYPE 9 ROOM
void lurk_room(int skt, int mode, void* pkt){
	if(mode == 0){
		struct room* outroom = (struct room*)pkt;
		int type = 9;
		write(skt, &type, 1);
		write(skt, &outroom->number, 2);
		write(skt, &outroom->name, 32);
		write(skt, &outroom->descl, 2);
		write(skt, &outroom->desc, outroom->descl);
	}	
	else if(mode == 1){
		struct room* inroom = (struct room*)pkt;
		read(skt, &inroom->number, 2);
		read(skt, &inroom->name, 32);
		read(skt, &inroom->descl, 2);
		read(skt, &inroom->desc, inroom->descl);
	}
}

//TYPE 10 CHARACTER
void lurk_character(int skt, int mode, void* pkt){
	if(mode == 0){
		struct character* outcharacter = (struct character*)pkt;
		int type = 10;
		write(skt, &type, 1);
		write(skt,&outcharacter->name,32);
		write(skt,&outcharacter->flags,1);
		write(skt,&outcharacter->attack,2);
		write(skt,&outcharacter->defence,2);
		write(skt,&outcharacter->regen,2);
		write(skt,&outcharacter->health,2);
		write(skt,&outcharacter->gold,2);
		write(skt,&outcharacter->room,2);
		write(skt,&outcharacter->descl,2);
		write(skt,&outcharacter->desc,outcharacter->descl);
	}
	else if(mode == 1){
		struct character* incharacter = (struct character*)pkt;
		read(skt,&incharacter->name,32);
		read(skt,&incharacter->flags,1);
		read(skt,&incharacter->attack,2);
		read(skt,&incharacter->defence,2);
		read(skt,&incharacter->regen,2);
		read(skt,&incharacter->health,2);
		read(skt,&incharacter->gold,2);
		read(skt,&incharacter->room,2);
		read(skt,&incharacter->descl,2);
		read(skt,&incharacter->desc,incharacter->descl);
	}
}

//TYPE 11 GAME
void lurk_game(int skt, int mode, void* pkt){
	if(mode == 0){
		struct game* outgame = (struct game*)pkt;
		int type = 11;
		write(skt, &type, 1);
		write(skt, &outgame->ipoints, 2);
		write(skt, &outgame->limit, 2);
		write(skt, &outgame->descl, 2);
		write(skt, &outgame->desc, outgame->descl);
		
	}
	else if(mode == 1){
		struct game* ingame = (struct game*)pkt;
		read(skt, &ingame->ipoints, 2);
		read(skt, &ingame->limit, 2);
		read(skt, &ingame->descl, 2);
		read(skt, &ingame->desc, ingame->descl);
	}
}

//TYPE 12 LEAVE ONLY USED BY CLIENT
void lurk_leave(int skt){
	int type = 12;
	write(skt,&type,1);
}

//TYPE 13 CONNECTION NOTE: Mostly the same to ROOM just different type
void lurk_connection(int skt, int mode, void* pkt){
	if(mode == 0){
		struct room* outconnection = (struct room*)pkt;
		int type = 13;
		write(skt, &type, 1);
		write(skt, &outconnection->number, 2);
		write(skt, &outconnection->name, 32);
		write(skt, &outconnection->descl, 2);
		write(skt, &outconnection->desc, outconnection->descl);
	}	
	else if(mode == 1){
		struct room* inconnection = (struct room*)pkt;
		read(skt, &inconnection->number, 2);
		read(skt, &inconnection->name, 32);
		read(skt, &inconnection->descl, 2);
		read(skt, &inconnection->desc, inconnection->descl);
	}
}
