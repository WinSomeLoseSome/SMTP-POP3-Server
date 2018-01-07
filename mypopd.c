#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024

static void handle_client(int fd);

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
        return 1;
    }

    run_server(argv[1], handle_client);

    return 0;
}

void handle_client(int fd) {

    // TODO To be implemented
    //intial welcome
    enum states {
        INITIALIZATION, USERAUTHORIZATION, PASSAUTHORIZATION, UPDATE, TRANSACTION
    };

    enum states state = INITIALIZATION;
    //send intial connection string
    send_string(fd, "+OK POP3 server ready \n");
    //create a netbuffer
    //create a net buffer
    net_buffer_t netbuf = nb_create(fd, MAX_LINE_LENGTH);
    //get the line
    char line[MAX_LINE_LENGTH];
    //TODO change back for push
    state = USERAUTHORIZATION;
    //create user and pass buffers to store username and password in
    char userBuff[MAX_LINE_LENGTH];
    char passBuff[MAX_LINE_LENGTH];

    int transLoop = 1;
    int authLoop = 1;
    while (authLoop) {
//////////////////// AUTHORIZATION STATE /////////////////////////////////////
        //for readability and understanding - is always true
        if (state == USERAUTHORIZATION) {
            //ask for USER and store the value in userBuff
            send_string(fd, "USER: ");
            nb_read_line(netbuf, line);
            strncpy(userBuff, line, strlen(line) - 1);
            userBuff[strlen(line) - 1] = '\0';

            //check if the user name is valid
            if (is_valid_user(userBuff, NULL) != 0) {
                send_string(fd, "+OK \n");
                state = PASSAUTHORIZATION;
                // if QUIT is called exit program
            } else if (strcmp(userBuff, "QUIT\0") == 0) {
                send_string(fd, "+OK\n");
                authLoop = 0;
                transLoop = 0;
            } else {
                send_string(fd, "-ERR \n");
            }
        } else if (state == PASSAUTHORIZATION) {
            //ask for PASSWORD and store the value in passBuff
            send_string(fd, "PASS: ");
            nb_read_line(netbuf, line);
            strncpy(passBuff, line, strlen(line) - 1);
            passBuff[strlen(line) - 1] = '\0';

            //check if the user name and password are valid
            if (is_valid_user(userBuff, passBuff) != 0) {
                send_string(fd, "+OK \n");
                state = TRANSACTION;
                authLoop = 0;
                // if QUIT is called exit program
            } else if (strcmp(passBuff, "QUIT\0") == 0) {
                send_string(fd, "+OK\n");
                authLoop = 0;
                transLoop = 0;
            } else {
                send_string(fd, "-ERR \n");
            }

        }
    }

    //get list of mail for user
    mail_list_t mail_list = load_user_mail(userBuff);
    if (mail_list == NULL) {
        //if no mail for user
    }

    //counts how many deletes have been called
    int deletes = 0;

////////////////////// TRANSACTION STATE /////////////////////////////////
    while(transLoop){
        if (state == TRANSACTION) {
            //read line and store in commandBuff and store first four char in argCommandBuff
            nb_read_line(netbuf, line);
            char commandBuff[MAX_LINE_LENGTH];
            char argCommandBuff[MAX_LINE_LENGTH];
            char argBuff[MAX_LINE_LENGTH];
            strcpy(argBuff, line + 5);
            strncpy(argCommandBuff, line, 5);
            strncpy(commandBuff, line, strlen(line) - 1);
            commandBuff[strlen(line) - 1] = '\0';

            //get mail count and size
            unsigned int mailCount = get_mail_count(mail_list);
            size_t mailSize = get_mail_list_size(mail_list);

            //when STAT command is received print +OK number of messages size of messages
            if (strcmp(commandBuff, "STAT\0") == 0) {
                send_string(fd, "+OK %d %d\n", mailCount, mailSize);

            //when LIST command is received
            } else if (strcmp(argCommandBuff, "LIST ") == 0 || strcmp(commandBuff, "LIST\0") == 0) {
                //for just LIST call
                if (strcmp(commandBuff, "LIST\0") == 0){

                    int i = mailCount+deletes;
                    send_string(fd, "+OK %d messages (%d octets) \n", mailCount, mailSize);
                    while (i >= 0 ) {
                        mail_item_t mail_item = get_mail_item(mail_list, i);
                        if (!mail_item){
                            //mail item doesnt exist or is marked as deleted
                        } else {
                            int index = getIndex(mail_item);
                            size_t item_size = get_mail_item_size(mail_item);

                            send_string(fd, "%d %d \n", index, item_size);
                        }
                        i--;
                    }
                    send_string(fd, ". \n");
                //for LIST arg call
                } else {
                    //get the arg that LIST is called with
                    int arg = getArg(line);
                    mail_item_t mail_item = NULL;
                    //this goes through the items and gets the one who's ID corresponds to the arguments
                    int i = 0;
                    while (i < mailCount + deletes){
                        mail_item = get_mail_item(mail_list, i);
                        if (!mail_item){

                        } else {
                            int index = getIndex(mail_item);
                            if (index == arg) {
                                break;
                            }
                        }
                        mail_item = NULL;
                        i++;
                    }

                    //if the item does not exist
                    if (arg == -2) {
                      send_string(fd, "-ERR too any parameters \n");
                    } else if (!mail_item) {
                        send_string(fd, "-ERR no such message, only %d messages in maildrop \n", mailCount);
                    //if it does exist
                    } else {
                        //indexes reversed, might be because of way they are stored
                        int index = getIndex(mail_item);
                        size_t item_size = get_mail_item_size(mail_item);
                        send_string(fd, "+OK %d %d \n", index, item_size);
                    }
                }


            } else if (strcmp(argCommandBuff, "RETR ") == 0 || strcmp(commandBuff, "RETR\0") == 0) {
                int arg = getArg(line);
                mail_item_t mail_item = NULL;
                //this goes through the items and gets the one who's ID corresponds to the arguments
                int i = 0;
                while (i < mailCount + deletes){
                    mail_item = get_mail_item(mail_list, i);
                    if (!mail_item){

                    } else {
                        int index = getIndex(mail_item);
                        if (index == arg) {
                            break;
                        }
                    }
                    mail_item = NULL;
                    i++;
                }

                if (arg == -2) {
                    send_string(fd, "-ERR too any parameters \n");
                } else if (!mail_item) {
                    send_string(fd, "-ERR no such message, only %d messages in maildrop \n", mailCount);
                } else {
                    int itemSize = get_mail_item_size(mail_item);
                    send_string(fd, "+OK %d octets \n", itemSize);
                    //get file name of mail item
                    char *pathName = get_mail_item_filename(mail_item);
                    char fileName[MAX_LINE_LENGTH];
                    strcpy(fileName, pathName);
                    //open the file
                    FILE *file;
                    file = fopen(fileName, "r");
                    int c;
                    //read the file
                    if (file) {
                        while ((c = getc(file)) != EOF) {
                            send_string(fd, "%c", c);
                        }
                        fclose(file);
                    }
                    //start new line for input
                    send_string(fd, "\n");
                }

            } else if (strcmp(argCommandBuff, "DELE ") == 0 || strcmp(commandBuff, "DELE\0") == 0) {
                int arg = getArg(line);
                mail_item_t mail_item = NULL;
                //this goes through the items and gets the one who's ID corresponds to the arguments
                int i = 0;
                while (i < mailCount + deletes){
                    mail_item = get_mail_item(mail_list, i);
                    if (!mail_item){

                    } else {
                        int index = getIndex(mail_item);
                        if (index == arg) {
                            break;
                        }
                    }
                    mail_item = NULL;
                    i++;
                }
                //if mail doesnt exist send error
                if (!mail_item) {
                    send_string(fd, "-ERR no such message\n");
                //if mail does exist send OK and mark as deleted
                } else {
                    mark_mail_item_deleted(mail_item);
                    send_string(fd, "+OK message deleted\n");
                    //update counter for deletes
                    ++deletes;
                }


            } else if (strcmp(commandBuff, "RSET\0") == 0) {

                reset_mail_list_deleted_flag(mail_list);
                unsigned int mailCount = get_mail_count(mail_list);
                send_string(fd, "+OK maildrop has %d messages\n", mailCount);
                deletes = 0;

            } else if (strcmp(commandBuff, "NOOP\0") == 0) {
                send_string(fd, "+OK \n");

            } else if (strcmp(commandBuff, "QUIT\0") == 0) {
                send_string(fd, "+OK\n");
                destroy_mail_list(mail_list);
                transLoop = 0;
            } else {
                send_string(fd, "-ERR\n");
            }


        }
    }
}

int getIndex(mail_item_t item) {
    char *pathName = get_mail_item_filename(item);
    char *fileName = strrchr(pathName, '/');
    while (*fileName) { // While there are more characters to process...
        if (isdigit(*fileName)) { // Upon finding a digit, ...
            long val = strtol(fileName, &fileName, 10); // Read a number, ...
            return val;
        } else { // Otherwise, move on to the next character.
            fileName++;
        }
    }
    return -1;
}

int getArg(char str[]){
    long val = -1;
    char *p = str + 5;
    int space = 0;
    while (*p) {
        if (isdigit(*p)){
            val = strtol(p, &p, 10);
        } else if (isspace(*p)){
            //check for extraneous args
            space++;
            if (space > 1) {
                return -2;
            }
            p++;
        } else {
            p++;
        }
    }
    return val;
}

