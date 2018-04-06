# LurkWolf
Lurk 2.0 Client written in C using ncurses UI

Alexander Romero readme for Lurk Client "LurkWolf"

Compile line:

gcc lurkwolf_main.c lurk_protocol.c -lpthread -lncurses -olurkwolf


Overview:

This is a Lurk Protocol client that has great features like: 
	Information Windows:
		You can easily see what is around you by setting up the extra info windows to whatever you need. Seeing your data was never easier!
	
Friends:
		Keep track of your prefered players easily and be able to message them with ease.

Detailed Output:
		Keep updated with what is happening around you with detailed info and plenty of look commands to see everything you may want to look at.

Moo:
		Moo


Commands:

/quit: Nicely closes out of the client with confirmation

/make: Has you input a player character to be sent

/login: Shorter version of make that uses default values other than name which you input

/start: Tells server client wants to start game

/msg: Lets you send a message to a player(entering text with no '/' will do a msg command with an empty recipient name)

/msgl: Similar to msg but gives you a numbered list of players so you can enter a number instead of an annoying username

/joinbattle: Supposed to change the current player character to join battles server withholding. Not tested fully

/fight: Sends a fight command to server. If anything happens it should tell you

/pvp: Lets you enter a player name to fight

/pvpl: Similar to pvp but gives you a numbered list of players so you can enter a number instead of an annoying username

/loot: Lets you loot a character whose name you enter

/lootlp: Similar to loot but gives you a numbered list of players so you can enter a number instead of an annoying username

/lootle: Similar to loot but gives you a numbered list of enemies so you can enter a number instead of an annoying name

/goto: Lets you go to a connection available to you in the room

/gotonum: Same as goto except you use the ID number for the room you want to go to

/gotol: similar to goto but gives you a numbered list of rooms so you don't even have to use gotonum

/goback: similar to goto but instantly takes you back to the previous room as long as the connection exists

/look: Tells you your current room and how many players, enemies, and connections are available

/lookat: Gets you details about players, enemies, and rooms or a list of each

/addfriend: Lets you enter a name of an available player to add to friend list

/addfriendl: Similar to addfriend but gives you a numbered list of players so you can enter a number instead of an annoying name

/popfriend: Removes last added friend

/clrfriend: Removes all friends

/friendmsg: Enables quick message for ease of messaging(When enabled any plain text you enter will be sent exclusivly to all friends added)

/win1player: Change window to player view(substitute 1 for 2 for the other window)

/win1enemy: Change window to enemy view(substitute 1 for 2 for the other window)

/win1room: Change window to room/connection view(substitute 1 for 2 for the other window)

/win1friend: Change window to friend view(substitute 1 for 2 for the other window)

/win1detail: Toggles window to show details(substitute 1 for 2 for the other window)

/win1activep: Toggle window to show active players only(Those who are not dead with health over 0)(substitute 1 for 2 for the other window)

/win1down: Scroll list down(substitute 1 for 2 for the other window)

/win1up: Scroll list up(substitute 1 for 2 for the other window)

/win1reset: Resets scroll to 0(substitute 1 for 2 for the other window)

/itobstr: For testing only, give it an integer and it will return a binary conversion(Might have a memory leak)

/verbose: Enables verboseness good for debugging and the such
