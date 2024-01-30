#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <dirent.h>

#define CONTROL_PORT 9010
#define DATA_PORT 9006
#define MAX_COMMAND_SIZE 1024
#define MAX_CLIENTS 15
#define MAX_STRING_SIZE 1024

// Declare response messages
char *user_message = "331 Username OK, need password";
char *login_fail_message = "530 Not logged in";
char *login_message = "230 User logged in, proceed";
char *port_message = "200 PORT command successful";
char *file_message = "150 File status okay; about to open data connection";
char *transfer_message = "226 Transfer complete";
char *dir_change_message = "200 directory changed to";
char *close_message = "221 Service closing control connection";
char *invalid_filename = "550 No such file or directory";
char *command_DNE = "202 Command not implemented";
char *PWD_message = "257";
char *bad_seq = "503 Bad sequence of commands";

// Declare supported Command List
const char *command_arr[] = {"LIST", "PORT", "STOR", "RETR", "PWD", "CWD", "USER", "PASS", "QUIT"};

// Keep a reference to base directory
char base_dir[MAX_STRING_SIZE];

// four global arrays with index as fd number to keep track of user info
char usernames[MAX_CLIENTS][MAX_STRING_SIZE] = {{0}};
char passwords[MAX_CLIENTS][MAX_STRING_SIZE] = {{0}};
char current_directory[MAX_CLIENTS][MAX_STRING_SIZE] = {{0}};
int loginStatus[MAX_CLIENTS] = {0}; // two levels : 0 - not connected, 1 - correct password, login success

// helper function in sending response message
void send_response(int client_socket, const char *response)
{
    send(client_socket, response, strlen(response), 0);
}

// helper method to read csv, return password if username is valid, else return Null
const char *checkUsername(char *username)
{
    FILE *file = fopen("users.csv", "r");
    if (file == NULL)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    // loop to find matching username
    char line[256];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *token = strtok(line, ",");
        if (token != NULL && strcmp(token, username) == 0)
        {
            // parse out only password
            token = strtok(NULL, ",");
            fclose(file);
            token[strcspn(token, "\n")] = '\0';
            return token; // Return matching password
        }
    }

    fclose(file);
    return NULL; // Username not found
}

// helper function check is command is valid or not
int isCommandValid(char *input)
{
    for (size_t i = 0; i < sizeof(command_arr) / sizeof(command_arr[0]); ++i)
    {
        if (strcmp(input, command_arr[i]) == 0)
        {
            return 1; // Command is valid
        }
    }
    return 0; // Command is not valid
}

// utility function for opening socket on port 21
int openDataSocket(struct sockaddr_in client_data_address)
{
    // Create data socket
    int data_socket = socket(AF_INET, SOCK_STREAM, 0);
    // setsocket
    int value = 1;
    setsockopt(data_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
    if (data_socket == -1)
    {
        perror("Data socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize port for control connection
    struct sockaddr_in data_address;
    memset(&data_address, 0, sizeof(data_address));
    data_address.sin_family = AF_INET;
    data_address.sin_addr.s_addr = htonl(INADDR_ANY);
    data_address.sin_port = htons(DATA_PORT);

    // Bind data_socket with data_addr
    if (bind(data_socket, (struct sockaddr *)&data_address, sizeof(data_address)) == -1)
    {
        perror("Data socket bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Connecting to Client Transfer Socket...\n");

    //  Establish data connection to client from data socket port 20
    if (connect(data_socket, (struct sockaddr *)&client_data_address, sizeof(client_data_address)) == -1)
    {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
    printf("Connection Successful\n");
    return data_socket;
}

int main()
{
    int control_socket, data_socket;
    struct sockaddr_in control_address, client_data_address;

    memset(&control_address, 0, sizeof(control_address)); // Reset

    // Create control socket
    control_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (control_socket == -1)
    {
        perror("Control socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize port for control connection
    control_address.sin_family = AF_INET;
    control_address.sin_addr.s_addr = INADDR_ANY;
    control_address.sin_port = htons(CONTROL_PORT);

    // Bind socket to address
    if (bind(control_socket, (struct sockaddr *)&control_address, sizeof(control_address)) == -1)
    {
        perror("Control socket bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(control_socket, 5) == -1)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    fd_set all_sockets;
    fd_set ready_sockets;

    int max_socket_so_far = control_socket;

    FD_ZERO(&all_sockets);
    FD_SET(control_socket, &all_sockets);

    printf("Server is listening...\n");

    // set base directory path
    getcwd(base_dir, sizeof(base_dir));

    while (1)
    {
        ready_sockets = all_sockets;

        if (select(max_socket_so_far + 1, &ready_sockets, NULL, NULL, NULL) < 0)
        {
            perror("select error");
            exit(EXIT_FAILURE);
        }

        for (int fd = 3; fd <= max_socket_so_far; fd++)
        {
            // Reset base_dir for every client
            chdir(base_dir);

            if (FD_ISSET(fd, &ready_sockets))
            {
                if (fd == control_socket) // incoming new connection on server socket
                {
                    int client_sd = accept(control_socket, 0, 0);
                    printf("Connection established with user %d \n", client_sd);

                    FD_SET(client_sd, &all_sockets);

                    // Send Welcome Message
                    char *response = "220 Service ready for the new user.\r\n";
                    send_response(client_sd, response);

                    // update max_socket_so_far
                    if (client_sd > max_socket_so_far)
                        max_socket_so_far = client_sd;
                }
                else // new activity on connected socket
                {
                    char recv_buffer[256];
                    bzero(recv_buffer, sizeof(recv_buffer));
                    recv(fd, recv_buffer, sizeof(recv_buffer), 0); // Receive input from client

                    char command[128];
                    char input[128];
                    bzero(command, sizeof(command));
                    bzero(input, sizeof(input));

                    // parse response into command and input
                    if (sscanf(recv_buffer, "%s %[^\n]", command, input) < 1)
                    {
                        if (strlen(recv_buffer) == 0) // Quit when client enters Ctrl+C
                        {
                            send_response(fd, close_message);
                            loginStatus[fd] = 0;
                            printf("Closed! \n");
                            close(fd);
                            FD_CLR(fd, &all_sockets); // remove socket from list
                            continue;
                        }
                        send_response(fd, command_DNE);
                        continue;
                    }

                    // Handle quit
                    if (strcmp(command, "QUIT") == 0)
                    {
                        send_response(fd, close_message);
                        loginStatus[fd] = 0;
                        for (int i = 0; i < MAX_STRING_SIZE; i++)
                        {
                            usernames[fd][i] = 0;
                        }
                        printf("Closed! \n");
                        close(fd);
                        FD_CLR(fd, &all_sockets); // remove socket from list
                        continue;
                    }

                    // Check if command is valid or not
                    if (isCommandValid(command) == 0)
                    {
                        send_response(fd, command_DNE);
                        continue;
                    }

                    // Handle username and password when client is not logged in
                    if (loginStatus[fd] == 0)
                    {
                        if (strcmp(command, "USER") == 0)
                        {
                            const char *pass = checkUsername(input);
                            if (pass != NULL) // if username is valid, fill in client info in global arrays
                            {
                                printf("Successful username verification\n\n");
                                strncpy(usernames[fd], input, strlen(input) + 1);
                                strncpy(passwords[fd], pass, strlen(pass) + 1);
                                send_response(fd, user_message);
                            }
                            else
                            {
                                send_response(fd, login_fail_message);
                            }
                        }
                        else if (strcmp(command, "PASS") == 0)
                        {
                            if (strlen(passwords[fd]) == 0) // If PASS is sent before USER, password not initilized yet
                            {
                                send_response(fd, bad_seq);
                            }
                            else if (strcmp(passwords[fd], input) == 0) // password matches, logged in
                            {
                                send_response(fd, login_message);

                                // update login status
                                loginStatus[fd] = 1;

                                // update directory info
                                chdir("server");
                                chdir(usernames[fd]);
                                strncpy(current_directory[fd], usernames[fd], strlen(usernames[fd]) + 1);

                                printf("Successful login\n\n");
                            }
                            else // password not matched
                            {
                                printf("Incorrect password\n\n");
                                send_response(fd, login_fail_message);
                            }
                        }
                        else // Other command sent before logged in -> bad sequence
                        {
                            send_response(fd, bad_seq);
                        }
                    }
                    else // if the client logged in
                    {
                        // set the directory to current working directory
                        chdir("server");
                        chdir(current_directory[fd]);

                        // Handle PWD command
                        if (strcmp(command, "PWD") == 0)
                        {
                            char temp_message[MAX_STRING_SIZE];
                            bzero(temp_message, sizeof(temp_message));

                            // construct pwd message with current working directory
                            if (snprintf(temp_message, MAX_STRING_SIZE, "%s %s/", PWD_message, current_directory[fd]) >= MAX_STRING_SIZE)
                            {
                                fprintf(stderr, "Error: Formatted string is too large.\n");
                                exit(EXIT_FAILURE);
                            }
                            printf("%s\n", temp_message);

                            send_response(fd, temp_message);
                        }

                        // Handle CWD command
                        else if (strcmp(command, "CWD") == 0)
                        {
                            // Client can't go back further than their own directory
                            if (strcmp(input, "..") == 0 && strcmp(current_directory[fd], usernames[fd]) == 0)
                            {
                                send_response(fd, "No Access Right\n");
                                continue;
                            }
                            else if (chdir(input) == 0)
                            {
                                // if exist, update current dir for only path after ../server/
                                char t[1024];
                                getcwd(t, sizeof(t));
                                char *substring = strstr(t, "server");

                                strncpy(current_directory[fd], substring + strlen("server/"), strlen(substring + strlen("server/")) + 1);

                                printf("Change directory to: %s\n\n", current_directory[fd]);

                                char temp_message[MAX_STRING_SIZE];
                                bzero(temp_message, sizeof(temp_message));
                                // construct pwd message with current working directory
                                if (snprintf(temp_message, MAX_STRING_SIZE, "%s %s/", dir_change_message, current_directory[fd]) >= MAX_STRING_SIZE)
                                {
                                    fprintf(stderr, "Error: Formatted string is too large.\n");
                                    exit(EXIT_FAILURE);
                                }
                                send_response(fd, temp_message);
                            }
                            else
                            {
                                send_response(fd, invalid_filename);
                                continue;
                            }
                        }

                        // Handle PORT command, will be sent before LIST, STOR, RETR
                        else if (strcmp(command, "PORT") == 0)
                        {
                            int ip[4];
                            int p1, p2;
                            // Parse the IP address and port values
                            sscanf(recv_buffer, "PORT %d,%d,%d,%d,%d,%d", &ip[0], &ip[1], &ip[2], &ip[3], &p1, &p2);
                            memset(&client_data_address, 0, sizeof(client_data_address));
                            // Store client_data_addr
                            client_data_address.sin_family = AF_INET;
                            client_data_address.sin_port = htons(p1 * 256 + p2); // Combine port values
                            client_data_address.sin_addr.s_addr = htonl((ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | ip[3]);

                            printf("Port received: %s\n\n", recv_buffer);
                            // send response to client
                            char *response = "200 PORT command successful.";
                            send_response(fd, response);

                            // Initiate new buff for LIST, RETR, STOR
                            char recv_buff[128];
                            bzero(recv_buff, sizeof(recv_buff));
                            recv(fd, recv_buff, sizeof(recv_buff), 0);

                            char port_command[128];
                            char port_input[128];
                            bzero(port_command, sizeof(port_command));
                            bzero(port_input, sizeof(port_input));

                            // parse response into command and input
                            if (sscanf(recv_buff, "%s %[^\n]", port_command, port_input) < 1)
                            {
                                send_response(fd, command_DNE);
                                continue;
                            }

                            if (strcmp(port_command, "LIST") == 0)
                            {
                                DIR *directory;
                                struct dirent *entry;
                                // Open the current directory
                                directory = opendir(".");
                                if (directory == NULL)
                                {
                                    send_response(fd, invalid_filename);
                                }
                                else
                                {
                                    // File Status OKAY, send 150 response
                                    send_response(fd, file_message);
                                    printf("File okay, beginning data connections\n\n");
                                    int pid = fork(); // fork a new process to handle data transfer
                                    if (pid == 0)
                                    {
                                        // close parent socket
                                        close(control_socket);
                                        // Open data socket
                                        int data_socket = openDataSocket(client_data_address);

                                        // Concatenate directory entries into a string
                                        char file_list[1024];
                                        bzero(file_list, sizeof(file_list));
                                        while ((entry = readdir(directory)) != NULL)
                                        {
                                            // ignore . and .. and any file starts with .
                                            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 && entry->d_name[0] != '.')
                                            {
                                                strcat(file_list, entry->d_name);
                                                strcat(file_list, "\n");
                                            }
                                        }
                                        // Close the directory
                                        closedir(directory);
                                        // Send file list using data channel
                                        int bytes_sent = send(data_socket, file_list, sizeof(file_list), 0);
                                        if (bytes_sent < 0)
                                        {
                                            perror("Send error");
                                        }
                                        printf("Listing directory\n");
                                        printf("%s\n\n", file_list);
                                        close(data_socket);
                                        // send 226 transfer complete response
                                        send_response(fd, transfer_message);
                                        exit(EXIT_SUCCESS);
                                    }
                                }
                            }
                            // Handle STOR command
                            else if (strcmp(port_command, "STOR") == 0)
                            {
                                // File Status OKAY, send 150 response
                                send_response(fd, file_message);
                                printf("File okay, beginning data connections\n\n");
                                int pid = fork(); // fork a new process to handle data transfer
                                if (pid == 0)
                                { // if it's the child process
                                    close(control_socket);
                                    int data_socket = openDataSocket(client_data_address); // Server open data port
                                    FILE *file = fopen(port_input, "wb");
                                    if (file == NULL)
                                    {
                                        perror("Open file error");
                                        fclose(file);
                                        continue;
                                    }
                                    int bytesRead;
                                    char file_buff[128];
                                    bzero(file_buff, sizeof(file_buff));
                                    while (1)
                                    { // read data from file and send
                                        bytesRead = recv(data_socket, file_buff, sizeof(file_buff), 0);
                                        if ((bytesRead <= 0))
                                        {
                                            printf("END\n");
                                            break;
                                        }
                                        fwrite(file_buff, 1, bytesRead, file);
                                        bzero(file_buff, sizeof(file_buff));
                                    }
                                    fclose(file);
                                    close(data_socket);
                                    // send success 226 response
                                    send_response(fd, transfer_message);
                                    exit(EXIT_SUCCESS);
                                }
                            }
                            // Handle RETR
                            else if (strcmp(port_command, "RETR") == 0)
                            {
                                if (fopen(port_input, "rb") == NULL)
                                {
                                    send_response(fd, invalid_filename);
                                    continue;
                                }
                                send_response(fd, file_message); // 150
                                printf("File okay, beginning data connections\n\n");
                                int pid = fork();
                                if (pid == 0)
                                {
                                    close(control_socket);
                                    data_socket = openDataSocket(client_data_address); // server open data port
                                    FILE *file = fopen(port_input, "rb");
                                    // Get the file size
                                    fseek(file, 0, SEEK_END);
                                    long fileSize = ftell(file);
                                    fseek(file, 0, SEEK_SET);
                                    char file_buffer[MAX_COMMAND_SIZE];
                                    bzero(file_buffer, sizeof(file_buffer));
                                    while (1)
                                    {
                                        int bytesRead = fread(file_buffer, 1, sizeof(file_buffer), file);
                                        printf("%d ", bytesRead);
                                        if (bytesRead <= 0)
                                        {
                                            break;
                                        }
                                        send(data_socket, file_buffer, bytesRead, 0);
                                    }
                                    fclose(file);
                                    close(data_socket);
                                    send_response(fd, transfer_message); // 226
                                    exit(EXIT_SUCCESS);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    // Close the control socket
    close(control_socket);
    return 0;
}
