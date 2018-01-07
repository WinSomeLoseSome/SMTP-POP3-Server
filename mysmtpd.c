#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024
#define MAX_MESSAGE_SIZE 10240

static void handle_client(int fd);
char* concat(const char *s1, const char *s2);

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
        return 1;
    }

    run_server(argv[1], handle_client);

    return 0;
}

void handle_client(int fd) {

    //Retrieving server name
    struct utsname unameData;
    uname(&unameData);
    char* serverName = concat(unameData.sysname, "\n");
    //send 220 if connection is ok
    char* initOutput= concat("220 Ready, server ", serverName);
    send_string(fd, initOutput);


    //create a net buffer
    net_buffer_t netbuf;
    netbuf = nb_create(fd, MAX_LINE_LENGTH);

    //get the line
    char line[MAX_LINE_LENGTH];

    //char arrays used to check the commands
    char helo[5] = {'H', 'E', 'L', 'O', '\0'};
    char ehlo[5] = {'E', 'H', 'L', 'O', '\0'};
    char quit[5] = {'Q', 'U', 'I', 'T', '\0'};
    char mail[5] = {'M', 'A', 'I', 'L', '\0'};
    char rcpt[5] = {'R', 'C', 'P', 'T', '\0'};
    char data[5] = {'D', 'A', 'T', 'A', '\0'};
    char noop[5] = {'N', 'O', 'O', 'P', '\0'};
    char fromArr[5] = {'F', 'R', 'O', 'M', '\0'};
    char toArr[5] = {'T', 'O', ':', '<', '\0'};


    int argNum = 0;
    char *addr = (char *) calloc(strlen(afterFour(line + 5)), sizeof(char));
    user_list_t *users = create_user_list();

    ///////////////////////// HELO Command Section ///////////////////////////////////////////

    while (1) {
        nb_read_line(netbuf, line);

        if ((checker(firstFour(line), helo) == 1) && argNum == 0) {

            send_string(fd, "250 Hello");
            //sends the domain if given
            send_string(fd, afterFour(line));
            argNum = 1;
            int loop = 1;

            //create a temp file

            char nameBuff[32];
            int filedes = -1;

            // memset the buffers to 0
            memset(nameBuff,0,sizeof(nameBuff));



            // Copy the relevant information in the buffers
            strncpy(nameBuff,"temp-XXXXXX",21);
            // Create the temporary file, this function will replace the 'X's
            filedes = mkstemp(nameBuff);




            while (loop) {
                nb_read_line(netbuf, line);
                int flag = notImplemented(line);
                int flag2 = notValidCommand(line);

     ///////////////////////// MAIL Command Section ///////////////////////////////////////////
                if (checker(firstFour(line), mail) == 1 && argNum == 1) {
                    //do stuff for MAIL here
                    char fromBuf[MAX_LINE_LENGTH];
                    memset(fromBuf,0,sizeof(fromBuf));
                    // check if there is a FROM area of input
                    char *from = firstFour(line + 5);
                    //check stuff after from
                    char *afterFrom = afterFour(line + 5);
                    //check if there is a FROM area in reply
                    if (checker(from, fromArr) == 1) {
                        //check for proper characters after FROM
                        if (afterFrom[0] == ':' && afterFrom[1] == '<') {
                            int j = 0;
                            //read the domain
                            while (afterFrom[j] != '>' && j != MAX_LINE_LENGTH - 6) { j++; }
                            //if there is no > to end domain send Invalid Format
                            if (j == MAX_LINE_LENGTH - 6) {
                                send_string(fd, "Invalid Format \n");
                                //otherwise send OK and wait for RCPT
                            } else {
                                //strcpy(fromBuf, line);
                                //write(filedes, fromBuf, MAX_LINE_LENGTH * sizeof(char));
                                //write(filedes, '\n', sizeof(char));
                                send_string(fd, "250 OK \n");
                                argNum = 2;
                            }

                        } else {
                            send_string(fd, "503 \n");
                        }
                    }

    ///////////////////////// RCPT Command Section ///////////////////////////////////////////
                } else if (checker(firstFour(line), rcpt) == 1 && argNum == 2) {
                    //do stuff for RCPT here
                    char toBuf[MAX_LINE_LENGTH];
                    memset(toBuf,0,sizeof(toBuf));
                    char *to = firstFour(line + 5);
                    char *afterTo = afterFour(line + 5);
                    //get the addr of the

                    char *start, *end;
                    start = strchr(line, '<');
                    end = strchr(line, '>');
                    char* trimmedDomain = malloc(20* sizeof(char));
                    strncpy(trimmedDomain, start + 1, end - start - 1);


                    strncpy(addr, afterFour(line + 5), strlen(afterFour(line + 5)) - 1);
                    //find the > char and remove from the string
                    /*int j = 0;
                    while (addr[j] != '>' && j != MAX_LINE_LENGTH - 4) { j++; }
                    if (j == MAX_LINE_LENGTH - 4) {
                        send_string(fd, "Invalid Format \n");
                        //otherwise send OK and wait for DATA
                    } else {
                        addr[j] = 0;
                    }*/
                    //check for TO:<
                    if (checker(to, toArr) == 1) {

                        if (is_valid_user(trimmedDomain, NULL) != 0){
                            argNum = 3;
                            //copy TO info to file
                            //strcpy(toBuf, line);
                            //write(filedes, toBuf, MAX_LINE_LENGTH * sizeof(char));
                            // write(filedes, '\n', sizeof(char));

                            send_string(fd, "250 OK \n");
                            add_user_to_list(&users, trimmedDomain);
                        } else {
                            send_string(fd, "550 no such user \n");
                            //send_string(fd, afterTo + 2);
                        }

                    } else {
                        send_string(fd, "503 \n");
                    }
   ///////////////////////// DATA Command Section ///////////////////////////////////////////
                } else if (checker(firstFourWithNullChar(line), data) == 1 && argNum == 3) {
                    //do stuff for DATA here
                    send_string(fd, "354 send message content; end with . \n");
                    argNum = 5;

                } else if (argNum == 5) {
                    //handle the DATA message
                    char buffer[MAX_LINE_LENGTH];
                    memset(buffer,0,sizeof(buffer));
                    char newLine = '\n';
                    strcpy(buffer, line);

                    //print data line to txt file
                    write(filedes, buffer, MAX_LINE_LENGTH * sizeof(char));
                    //write(filedes, newLine, sizeof(char));

                    // when . is received then
                    if (line[0] == '.') {
                        send_string(fd, "250 OK \n");
                        argNum = 1;
                        //problem with addr
                        save_user_mail(nameBuff, users);
                        // Call unlink so that whenever the file is closed or the program exits
                        // the temporary file is deleted
                        unlink(nameBuff);
                        close(filedes);

                    }

///////////////////////// NOOP Command Section ///////////////////////////////////////////
                } else if (checker(firstFourWithNullChar(line), noop) == 1) {
                    //do stuff for DATA here
                    send_string(fd, "250 OK \n");

///////////////////////// QUIT Command Section ///////////////////////////////////////////
                } else if (checker(line, quit) == 1) {
                    loop = 0;
                } else if (flag == 1){
                    send_string(fd, "502 Command not implemented\n");
                }else if (flag2 == 1){
                    send_string(fd, "500 Syntax error, command unrecognized\n");
                }
                else {
                    send_string(fd, "503 Bad sequence of commands. \n");
                }

            }
            send_string(fd, "221 closing connection to %s", serverName);
            argNum = 0;
            break;
        }


    }
}

//gets first four input chars and makes a string null byte at end
    int firstFour(char input[]) {
        char *four = malloc(4 * sizeof(char));
        if (!four) {
            //handle error
        }

        memcpy(four, input, 4 * sizeof(char));

        return four;

    }

    int firstFourWithNullChar(char input[]) {
        char *four = malloc(5 * sizeof(char));
        four[4] = '\0';
        if (!four) {
            //handle error
        }
        memcpy(four, input, 4 * sizeof(char));

        return four;

    }

//gets all the information after the first four
    int afterFour(char input[]) {
        char *afterFour = malloc(100 * sizeof(char));
        if (!afterFour) {
            //handle error
        }

        memcpy(afterFour, input + 4, 100 * sizeof(char));

        return afterFour;
    }


    int checker(char input[], char check[]) {
        int i, result = 1;
        for (i = 0; input[i] != '\0' && check[i] != '\0'; i++) {
            if (input[i] != check[i]) {
                result = 0;
                break;
            }
        }
        return result;
    }

    char* concat(const char *s1, const char *s2)
    {
        char *result = malloc(strlen(s1)+strlen(s2)+1);//+1 for the null-terminator
        strcpy(result, s1);
        strcat(result, s2);
        return result;
    }

    int notImplemented(char *com) {
        int flag = 0;
        if ( (strcmp("EHLO\r\n", com) == 0) ||
             (strcmp("RSET\r\n", com) == 0) ||
             (strcmp("VRFY\r\n", com) == 0) ||
             (strcmp("EXPN\r\n", com) == 0) ||
             (strcmp("HELP\r\n", com) == 0) )
        {
            flag = 1;
        }
        return flag;
    }

    int notValidCommand(char *com) {
        int flag = 0;
        if( (strcmp("HELO\r\n", com) != 0) ||
            (strcmp("MAIL\r\n", com) != 0) ||
            (strcmp("RCPT\r\n", com) != 0) ||
            (strcmp("DATA\r\n", com) != 0) ||
            (strcmp("NOOP\r\n", com) != 0) ||
            (strcmp("QUIT\r\n", com) != 0) )
        {
            flag = 1;
        }
        return flag;
    }


      /*  {send_string(fd, "502 Command not implemented\n");}
        else if( (strcmp("HELO\r\n", com) != 0) ||
                (strcmp("MAIL\r\n", com) != 0) ||
                (strcmp("RCPT\r\n", com) != 0) ||
                (strcmp("DATA\r\n", com) != 0) ||
                (strcmp("NOOP\r\n", com) != 0) ||
                 (strcmp("QUIT\r\n", com) != 0) )
        {
        send_string(fd, "500 Syntax error, command unrecognized\n");
    }*/
