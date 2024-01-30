#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_COMMAND_SIZE 1024
#define MAX_RESPONSE_SIZE 1024
#define CONTROL_PORT 9010
#define DATA_PORT 9006
#define REUSE_CAP 10

// helper method in receiving response
void receive_response(int client_control_socket)
{
    char response[MAX_RESPONSE_SIZE];
    bzero(response, sizeof(response));
    int bytes_received = recv(client_control_socket, response, sizeof(response), 0);
    if (bytes_received == -1)
    {
        perror("Receive failed");
        exit(EXIT_FAILURE);
    }
    printf("%s\n", response);
}

// helper method in sending message
void send_message(int client_control_socket, char *message)
{
    size_t message_length = strlen(message);
    // Send the actual message
    if (send(client_control_socket, message, message_length, 0) == -1)
    {
        perror("Error sending message");
        return;
    }
}

// helper method in parsing IP
void parseIP(char *ip_str, int *ip_parts)
{
    char *token = strtok(ip_str, ".");
    int i = 0;

    while (token != NULL && i < 4)
    {
        ip_parts[i++] = atoi(token);
        token = strtok(NULL, ".");
    }
}

// helper method in sending port number to server
int sendPort(int client_control_socket, int i)
{
    struct sockaddr_in client_control_address;
    memset(&client_control_address, 0, sizeof(client_control_address));
    socklen_t len = sizeof(client_control_address);

    // Using bound socket, find current ip and port
    if (getsockname(client_control_socket, (struct sockaddr *)&client_control_address, &len) == -1)
    {
        perror("Getsockname failed");
        exit(EXIT_FAILURE);
    }

    // Create new data socket
    int client_data_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_data_socket == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    // setsock
    int value = 1;
    setsockopt(client_data_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)); //&(int){1},sizeof(int)

    // Create new data address
    struct sockaddr_in client_data_address;
    memset(&client_data_address, 0, sizeof(client_data_address));
    client_data_address.sin_family = AF_INET;
    client_data_address.sin_addr.s_addr = client_control_address.sin_addr.s_addr;

    // Set Port N+i
    if (i <= REUSE_CAP)
    { // reuse
        client_data_address.sin_port = htons(ntohs(client_control_address.sin_port) + i);
    }
    else
    {
        client_data_address.sin_port = client_control_address.sin_port;
        i = 0;
    }

    if (bind(client_data_socket, (struct sockaddr *)&client_data_address, sizeof(client_data_address)) == -1)
    {
        perror("Error binding data socket");
        close(client_data_socket);
        exit(EXIT_FAILURE);
    }

    // Send command "PORT"
    // convert IP address to four decimals
    char send_buffer[MAX_RESPONSE_SIZE];
    bzero(send_buffer, sizeof(send_buffer));
    char *ip_buffer = inet_ntoa(client_data_address.sin_addr);
    int ip_parts[4];
    parseIP(ip_buffer, ip_parts);
    // convert port number to two decimals
    int p1 = ntohs(client_data_address.sin_port) / 256;
    int p2 = ntohs(client_data_address.sin_port) % 256;
    bzero(send_buffer, sizeof(send_buffer));
    sprintf(send_buffer, "PORT %d,%d,%d,%d,%d,%d", ip_parts[0], ip_parts[1], ip_parts[2], ip_parts[3], p1, p2);
    send_message(client_control_socket, send_buffer);
    return client_data_socket;
}

int main()
{
    // srtting up current client working directory
    char client_dir[MAX_COMMAND_SIZE];
    bzero(client_dir, sizeof(client_dir));
    chdir("client");
    getcwd(client_dir, sizeof(client_dir));

    // Some response check message
    char *login_message = "230 User logged in, proceed";
    char *close_message = "221 Service closing control connection";
    char *PORT_response = "200 PORT command successful.";
    char *file_response = "150 File status okay; about to open data connection";
    // an login status marker, 0 - not logged in, 1 - logged in
    int login_status = 0;

    // print opening message on client side
    printf("Hello!! Please Authenticate to run server commands\n"
           "1. type \"USER\" followed by a space and your username\n"
           "2. type \"PASS\" followed by a space and your password\n\n"
           "\"QUIT\" to close connection at any moment\n"
           "Once Authenticated\n"
           "this is the list of commands:\n"
           "\"STOR\" + space + filename |to send a file to the server\n"
           "\"RETR\" + space + filename |to download a file from the server\n"
           "\"LIST\" to list all the files under the current server directory\n"
           "\"CWD\" + space + directory |to change the current server directory\n"
           "\"PWD\" to display the current server directory\n"
           "Add \"!\" before the last three commands to apply them locally.\n");

    int data_socket;
    struct sockaddr_in control_address, data_address;

    // create client socket to send to server
    int client_control_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_control_socket == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // setsock
    int value = 1;
    setsockopt(client_control_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)); //&(int){1},sizeof(int)

    // Initialize server address struct
    control_address.sin_family = AF_INET;
    control_address.sin_addr.s_addr = INADDR_ANY;
    control_address.sin_port = htons(CONTROL_PORT);

    // Open Control Connection
    if (connect(client_control_socket, (struct sockaddr *)&control_address, sizeof(control_address)) == -1)
    {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // keep track of client_data_port and reuse after hitting reuse_cap
    int i = 0;

    // receive welcome message
    receive_response(client_control_socket);

    while (1)
    {
        char send_buffer[MAX_COMMAND_SIZE];
        bzero(send_buffer, sizeof(send_buffer));
        printf("\n");
        printf("ftp> ");
        // get user input
        fgets(send_buffer, sizeof(send_buffer), stdin);
        send_buffer[strcspn(send_buffer, "\n")] = '\0'; //Strip newline character

        char command[128];
        char input[128];
        bzero(command, sizeof(command));
        bzero(input, sizeof(input));

        // parse response into command and input
        if (sscanf(send_buffer, "%s %[^\n]", command, input) < 1)
        {
            fprintf(stderr, "Invalid input format\n");
            continue;
        }

        // handle QUIT
        if (strcmp(command, "QUIT") == 0)
        {
            // Send QUIT message
            send_message(client_control_socket, send_buffer);

            // Receive QUIT response from server
            char response[MAX_RESPONSE_SIZE];
            bzero(response, sizeof(response));
            int bytes_received = recv(client_control_socket, response, sizeof(response), 0);
            if (bytes_received == -1)
            {
                perror("Receive failed");
                exit(EXIT_FAILURE);
            }
            printf("%s\n", response);
            // Close
            if (strcmp(response, close_message) == 0)
            {
                // Close the connection
                close(client_control_socket);
                break;
            }
        }

        // check if the user is logged in
        if (login_status == 0)
        {
            // Send login info
            send_message(client_control_socket, send_buffer);
            char response[MAX_RESPONSE_SIZE];
            bzero(response, sizeof(response));
            int bytes_received = recv(client_control_socket, response, sizeof(response), 0);
            if (bytes_received == -1)
            {
                perror("Receive failed");
                exit(EXIT_FAILURE);
            }
            printf("%s\n", response);

            // Check for correct login response
            if (strcmp(response, login_message) == 0)
            {
                // update login status
                login_status = 1;
            }
        }
        else // Handle command after logged in
        {
            // Handle local commands
            if (command[0] == '!')
            {
                if (strcmp(command, "!LIST") == 0)
                {
                    // Execute command to list files in the "project/client" directory
                    if (system("ls") == -1)
                    {
                        perror("Error executing command");
                    }
                }
                else if (strncmp(command, "!CWD", 4) == 0)
                {
                    // User can't go back beyond client direction
                    if (strcmp(input, "..") == 0 && strcmp(strrchr(client_dir, '/') + 1, "client") == 0)
                    {
                        printf("No Access Right\n");
                    }
                    else
                    {
                        // input is the folder we want to get into
                        if (chdir(input) == 0)
                        {
                            getcwd(client_dir, sizeof(client_dir));
                            printf("Changed current directory to: %s\n", client_dir);
                        }
                        else
                        {
                            printf("No such file or directory\n");
                        }
                    }
                }
                else if (strcmp(command, "!PWD") == 0)
                {
                    // Display the current working directory
                    printf("Current directory: %s\n", client_dir);
                }else{
                    printf("No such command!\n");
                }
                continue;
            }
            else if (strcmp(command, "LIST") == 0 || strcmp(command, "STOR") == 0 || strcmp(command, "RETR") == 0)
            {
                i += 1;
                // store current port the client is using
                int client_data_socket = sendPort(client_control_socket, i); // PORT command: send IP and port to server
                char response[MAX_RESPONSE_SIZE];
                bzero(response, sizeof(response));

                // receive 200 response, check if PORT is open successfully
                recv(client_control_socket, response, sizeof(response), 0);
                printf("%s\n", response);
                if (strcmp(response, PORT_response) == 0)
                {
                    send_message(client_control_socket, send_buffer);
                }

                bzero(response, sizeof(response));
                // Receive 150 file okay response
                recv(client_control_socket, response, sizeof(response), 0);
                printf("%s\n", response);
                if (strcmp(response, file_response) != 0)
                {
                    printf("Command failed\n");
                    continue;
                }

                // Listen for incoming connections
                if (listen(client_data_socket, 5) == -1)
                {
                    perror("Listen failed");
                    exit(EXIT_FAILURE);
                }

                // accept data connection
                socklen_t addr_size = sizeof(data_address);
                int server_sd = accept(client_data_socket, (struct sockaddr *)&data_address,
                                       &addr_size);
                // Handle LIST
                if (strcmp(command, "LIST") == 0)
                {
                    // receive directory
                    char file_list[MAX_RESPONSE_SIZE];
                    bzero(file_list, sizeof(file_list));
                    if (recv(server_sd, file_list, sizeof(file_list), 0) == -1)
                    {
                        perror("Receive failed/n");
                        exit(EXIT_FAILURE);
                    }
                    printf("%s", file_list);
                    close(server_sd);
                    close(client_data_socket);
                    receive_response(client_control_socket);
                }

                // Handle STOR
                else if (strcmp(command, "STOR") == 0)
                {
                    FILE *file = fopen(input, "r");
                    if (file == NULL)
                    {
                        perror("File DNE!");
                        continue;
                    }

                    char file_buffer[MAX_COMMAND_SIZE];
                    bzero(file_buffer, sizeof(file_buffer));
                    while (1)
                    {
                        int bytesRead = fread(file_buffer, 1, sizeof(file_buffer), file);
                        if (bytesRead <= 0)
                        {
                            break;
                        }
                        send(server_sd, file_buffer, bytesRead, 0);
                    }
                    fclose(file);
                    close(server_sd);
                    close(client_data_socket);
                    receive_response(client_control_socket);
                }
                else if (strcmp(command, "RETR") == 0)
                {
                    // receive file
                    FILE *file = fopen(input, "wb"); 
                    if (file == NULL)
                    {
                        perror("Open file error");
                        fclose(file);
                        continue;
                    }
                    int bytesRead;
                    char recv_buffer[MAX_COMMAND_SIZE];
                    bzero(recv_buffer, sizeof(recv_buffer));
                    while (1)
                    {
                        bytesRead = recv(server_sd, recv_buffer, sizeof(recv_buffer), 0);
                        if (bytesRead <= 0)
                        {
                            break;
                        }
                        fwrite(recv_buffer, 1, bytesRead, file);
                        bzero(recv_buffer, sizeof(recv_buffer));
                    }
                    fclose(file);
                    close(client_data_socket);
                    close(server_sd);
                    receive_response(client_control_socket);
                }
            }
            else
            {
                send_message(client_control_socket, send_buffer);
                receive_response(client_control_socket);
            }
        }
    }
    return 0;
}
