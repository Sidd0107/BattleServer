/*
 * This is a server side battleServer.
 */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> 
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef PORT
#define PORT x
#endif

struct client {
    int fd;
    char name[30];  //Stores name of each client
    struct in_addr ipaddr;
    struct client *next;
    //Pointer to client previously played with
    struct client *previous_played;
    struct client *opponent; // Pointer to current opponent.
    //Stores condition. If player is playing 1 else 0.
    int playing;
    // Health Stores hitpoints of each client during a game
    int health;
    // Stores no. of Power moves
    int power_moves;
    // If it is the players turn value 1 else 0 during
    // the course of a game.
    int active;
    // Value of plyers attacking move.
    int attack;
    // Stores 1 if player enters s else 0.
    int speak;
};

static struct client *addclient(struct client *top, int fd, struct in_addr addr);
static struct client *removeclient(struct client *top, int fd);
int handleclient(struct client *p, struct client *top);
static void start_game(struct client *p1, struct client *p2);
static void match(struct client *p);
int generateRandom(int l, int u);
static void attack(struct client *p1, struct client *p2, struct client *top);
static void display_options(struct client *p1, struct client *p2);
static void end_game(struct client *p1, struct client *p2, struct client * top);
static void display_attack(struct client *p1, struct client *p2, struct client *top);
static void end_game_abnormally(struct client *p);

struct client *head = NULL;

int bindandlisten(void);

int main(void) {
    int clientfd, maxfd, nready;
    struct client *p;
    
    socklen_t len;
    struct sockaddr_in q;
    struct timeval tv;
    fd_set allset;
    fd_set rset;
    
    int i;
    
    srand((unsigned int)time(NULL));
    
    int listenfd = bindandlisten();
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;
    
    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;
        /* timeout in seconds (You may not need to use a timeout for
         * your assignment)*/
        tv.tv_sec = 10;
        tv.tv_usec = 0;  /* and microseconds */
        
        nready = select(maxfd + 1, &rset, NULL, NULL, &tv);
        if (nready == 0) {
            //printf("No response from clients in %ld seconds\n", tv.tv_sec);
            continue;
        }
        
        if (nready == -1) {
            perror("select");
            continue;
        }
        
        if (FD_ISSET(listenfd, &rset)){
            printf("a new client is connecting\n");
            len = sizeof(q);
            if ((clientfd = accept(listenfd, (struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                exit(1);
            }
            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("connection from %s\n", inet_ntoa(q.sin_addr));
            head = addclient(head, clientfd, q.sin_addr);
        }
        
        for(i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &rset)) {
                for (p = head; p != NULL; p = p->next) {
                    if (p->fd == i) {
                        int result = handleclient(p, head);
                        if (result == -1) {
                            int tmp_fd = p->fd;
                            head = removeclient(head, p->fd);
                            FD_CLR(tmp_fd, &allset);
                            close(tmp_fd);
                        }
                        break;
                    }
                }
            }
        }
    }
    return 0;
}

int handleclient(struct client *p, struct client *top) {
    char buf[256];
    int len = read(p->fd, buf, sizeof(buf) - 1);
    if (len > 0) {
        // If the player is in a game
        if(p ->  playing == 1){
            // It is the players turn
            if(p -> active == 1){
                // Player wants to speak(s entered)
                if(p -> speak == 1){
                    //Stores user input
                    char msg1[1024];
                    // Writes users input on oponents and players window based on format
                    sprintf(msg1, "\r\n%s says :", p -> name);
                    if(write(p -> opponent -> fd , msg1, strlen(msg1)) == -1) {
                        perror("write");
                        exit(1);
                    }
                    if(write(p -> opponent -> fd , buf, strlen(buf)) == -1) {
                        perror("write");
                        exit(1);
                    }
                    if(write(p -> fd , "\r\n\r\nYou Speak:", 14) == -1) {
                        perror("write");
                        exit(1);
                    }
                    if(write(p -> fd , buf, strlen(buf)) == -1) {
                        perror("write");
                        exit(1);
                    }
                    p -> speak = 0;
                    display_options(p, p -> opponent);
                    
                }
                // If user choses attack option
                if(buf[0] == 'a'){
                    // picks random value(helper function) between parameters
                    // and attacks
                    p -> attack = generateRandom(2, 6);
                    attack(p, p -> opponent, top);
                }
                // If user choses powermove option
                if((buf[0] == 'p') && (p -> power_moves > 0)) {
                    //Executes powermove based on randomly generated value
                    // between given bounds
                    p -> power_moves -= 1;
                    int probab = generateRandom(0, 1);
                    // Computes if attack is on target or misses.
                    if(probab == 1){
                        p -> attack = 3 * generateRandom(2, 6);
                    }else{
                        p -> attack = 0;
                    }
                    
                    attack(p, p -> opponent, top);
                }
                //If client wishes to speak, it conveys the message.
                if(buf[0] == 's'){
                    p -> speak = 1;
                    if(write(p -> fd , "\r\n\r\nSpeak:", 10) == -1) {
                        perror("write");
                        exit(1);
                    }
                    
                }
            }else{//Not the players turn
                //Empty block
            }
            
        }else{//Player not in a match
            //Empty block
        }
        
        return 0;
    } else if (len == 0) {
        // socket is closed
        // printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
        // sprintf(outbuf, "Goodbye %s\r\n", inet_ntoa(p->ipaddr));
        // broadcast(top, outbuf, strlen(outbuf));
        return -1;
    } else { // shouldn't happen
        perror("read");
        return -1;
    }
}

/*
 Attacks player p2 with damaage stored in p1->attack.
 This is done if p1 is active. Once completed, it toggles the values fo active.
 */
static void attack(struct client *p1, struct client *p2, struct client *top){
    p2 -> health -= p1 -> attack;
    p1 -> active = 0;
    p2 -> active = 1;
    display_attack(p1, p2, top);
    if(p2 -> health > 0){
        display_options(p2, p1);
    }
    
}
/*
 Does most of the diplay work on both screens based on the required format.
 It uses the data stored within the stuct of each client to do so.
 */
static void display_attack(struct client *p1, struct client *p2, struct client *top){
    // Prints attack messages based on the attack.
    if(p1 -> attack > 0){
        char msg1[1024];
        sprintf(msg1, "\r\nYou hit %s for %d damage", p2 -> name, p1 -> attack);
        if(write(p1 -> fd , msg1, strlen(msg1)) == -1) {
            perror("write");
            exit(1);
        }
        
        char msg2[1024];
        sprintf(msg2, "\r\n%s hits you for %d damage", p1 -> name, p1 -> attack);
        if(write(p2 -> fd , msg2, strlen(msg2)) == -1) {
            perror("write");
            exit(1);
        }
        //power move misses. Diplays the following message.
    }else{
        char msg1[1024];
        sprintf(msg1, "\r\nYou missed!");
        if(write(p1 -> fd , msg1, strlen(msg1)) == -1) {
            perror("write");
            exit(1);
        }
        
        char msg2[1024];
        sprintf(msg2, "\r\n%s missed you!", p1 -> name);
        if(write(p2 -> fd , msg2, strlen(msg2)) == -1) {
            perror("write");
            exit(1);
        }
    }
    // Player's health<=0 implies he lost. Displays message below.
    if(p2 -> health <= 0){
        char msg3[1024];
        sprintf(msg3, "\r\nYou are no match for %s", p1 -> name);
        if(write(p2 -> fd , msg3, strlen(msg3)) == -1) {
            perror("write");
            exit(1);
        }
        // Print's this on wining players screen.
        char msg4[1024];
        sprintf(msg4,  "\r\n%s gives up, You win!\r\n", p2 -> name);
        if(write(p1 -> fd , msg4, strlen(msg4)) == -1) {
            perror("write");
            exit(1);
        }
        end_game(p1, p2, top);
    }
    
}

/* bind and listen, abort on error
 * returns FD of listening socket
 */
int bindandlisten(void) {
    struct sockaddr_in r;
    int listenfd;
    
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);
    
    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }
    
    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
    return listenfd;
}

static struct client *addclient(struct client *top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }
    
    printf("Adding client %s\n", inet_ntoa(addr));
    
    if(write(fd, "What is your name? ", 20) == -1) {
        perror("write");
        exit(1);
    }
    if(read(fd, p->name, sizeof(p->name)) <= 0) {
        perror("read");
        exit(1);
    }
    p -> name[strlen(p -> name) -1] =  '\0';
    
    //Sets all values and initilizes the client.
    p->fd = fd;
    p->ipaddr = addr;
    p->next = top;
    p->previous_played = NULL;
    p -> opponent = NULL;
    p->playing = 0;
    p -> health = 0;
    p -> power_moves = 0;
    p -> attack = 0;
    p -> active = 0;
    p -> speak = 0;
    top = p;
    
    // Broadcasts Entry message.
    struct client *iter = head;
    while(iter){
        char msg[1024];
        sprintf(msg,  "\r\n**%s enters the arena**\r\n", p -> name);
        if(write(iter -> fd , msg, strlen(msg)) == -1) {
            perror("write");
            exit(1);
        }
        iter = iter -> next;
    }
    match(p);
    
    return top;
}

static struct client *removeclient(struct client *top, int fd) {
    struct client **p;
    struct client *opp;
    
    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        if ((*p)-> playing){
            (*p)-> health = -1;
            char msg4[1024];
            sprintf(msg4,  "\r\n%s gives up, You win!\r\n", (*p) -> name);
            if(write((*p) -> opponent -> fd , msg4, strlen(msg4)) == -1) {
                perror("write");
                exit(1);
            }
            opp = (*p) -> opponent;
        }
        struct client *iter = head;
        while(iter){
            char msg[1024];
            sprintf(msg,  "\r\n**%s leaves**\r\n", (*p) -> name);
            if(write(iter -> fd , msg, strlen(msg)) == -1) {
                perror("write");
                exit(1);
            }
            iter = iter -> next;
        }
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                fd);
    }
    end_game_abnormally(opp);
    return top;
}
/*
 In case a player quits a match, the following is done.
 */
static void end_game_abnormally(struct client *p){
    p -> previous_played = NULL;
    p -> playing = 0;
    p -> health = 0;
    p -> power_moves = 0;
    p -> active = 0;
    p -> opponent = NULL;
    
    match(p);
}

/*
 Finds a match for a player waiting for an opponent.
 */
static void match(struct client *p){
    struct client *iter = head;
    struct client *opp = NULL;
    //Iterates through linked list till a free player is found.
    while (iter != NULL){
        if(iter -> playing == 0){
            if(iter != p){
                //Makes sure these players didn't play thir last match together.
                if (iter -> previous_played != p){
                    if (p -> previous_played != iter){
                        opp = iter;
                        break;
                    }else{
                        iter = iter -> next;
                    }
                }else{
                    iter = iter -> next;
                }
            } else{
                iter = iter -> next;
            }
        } else{
            iter = iter -> next;
        }
    }
    //Displays message stating who the opponent is.
    if(opp){
        char msg1[1024];
        sprintf(msg1, "You engage %s", opp -> name) ;
        if(write(p -> fd , msg1, strlen(msg1)) == -1) {
            perror("write");
            exit(1);
        }
        char msg2[1024];
        sprintf(msg2, "You engage %s", p -> name );
        if(write(opp -> fd , msg2, strlen(msg2)) == -1) {
            perror("write");
            exit(1);
        }
        start_game(opp, p);
    } else{
        if(write(p -> fd, "\r\nWaiting for opponent...", 26) == -1) {
            perror("write");
            exit(1);
        }
        
    }
}
/*
 Initializes a game between 2 players matched together.
 */
static void start_game(struct client *p1, struct client *p2){
    // Randomly generates values between 2 integers based on description.
    // Sets the hitpoints, no. of powermoves and sets their values stating they are in a match.
    p1 -> health =  generateRandom(10, 30);
    p1 -> power_moves = generateRandom(1, 3);
    p1 -> playing = 1;
    p2 -> health =  generateRandom(10, 30);
    p2 -> power_moves = generateRandom(1, 3);
    p2 -> playing = 1;
    p1 -> active = 1;
    
    p1 -> opponent = p2;
    p2 -> opponent = p1;
    display_options(p1, p2);
}
/*
 Generates random values between 2 integers.
 */
int generateRandom(int l, int u)
{
    int r = rand() % ((u - l) + 1);
    r = l + r;
    return r;
}
/*
 Displays all options a player has based on the no. of powermoves remaining.
 */
static void display_options(struct client *p1, struct client *p2){
    char msg1[1024];
    sprintf(msg1, "Your hitpoints: %d\r\nYour powermoves: %d\r\n\r\n%s's hitpoints: %d", p1 -> health, p1 -> power_moves, p2 -> name, p2 -> health);
    
    if(write(p1 -> fd , msg1, strlen(msg1)) == -1) {
        perror("write");
        exit(1);
    }
    
    if(p1 -> power_moves > 0){
        char msg2[1024];
        sprintf(msg2,"\r\n(a)ttack\r\n(p)owermove\r\n(s)peak something\r\n");
        if(write(p1 -> fd , msg2, strlen(msg2)) == -1) {
            perror("write");
            exit(1);
        }
    }else{
        char msg2[1024];
        sprintf(msg2, "\r\n(a)ttack\r\n(s)peak something\r\n");
        if(write(p1 -> fd , msg2, strlen(msg2)) == -1) {
            perror("write");
            exit(1);
        }
    }
    
    char msg3[100];
    sprintf(msg3, "\r\nWaiting for %s to strike...", p1 -> name);
    if(write(p2 -> fd , msg3, strlen(msg3)) == -1) {
        perror("write");
        exit(1);
    }
}

/*
 Settles all values of the client when a game is over. It puts the player back
 to a situation where they can be rematched with new players.
 */
static void end_game(struct client *p1, struct client *p2, struct client * top){
    //Resets player 1's values
    p1 -> previous_played = p2;
    p1 -> playing = 0;
    p1 -> health = 0;
    p1 -> power_moves = 0;
    p1 -> active = 0;
    p1 -> opponent = NULL;
    
    //Resets player 2's values.
    p2 -> previous_played = p2;
    p2 -> playing = 0;
    p2 -> health = 0;
    p2 -> power_moves = 0;
    p2 -> active = 0;
    p2 -> opponent = NULL;
    
    match(p1);
    match(p2);
}
