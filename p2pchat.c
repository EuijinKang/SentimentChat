#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "socket.h"
#include "ui.h"
#include "strmap.h"
#include "sentiment.h"
// CITATIONS
// Paul Lu
// Linh Tang
// Sam Eagen - talked about storing fds
// Man Page Poll
// Time out programming using poll() system call Emory University CS557
// send message and receive message function taken from networking lab

// Maximum possible length of a user's message
#define MAX_MESSAGE_LENGTH 2048

//global variable for sentiment

double sentiment;

// A function that sends a message to a socket. Takes in a file descriptor
// and a message a user wishes to send.

int send_message(int fd, char *message) {
  // If the message is NULL, set errno to EINVAL and return an error.
  if (message == NULL) {
    errno = EINVAL;
    return -1;
  }

  // First, send the length of the message in a size_t.
  size_t len = strlen(message);
  if (write(fd, &len, sizeof(size_t)) != sizeof(size_t)) {
    // Writing failed, so return an error.
    return -1;
  }

  // Now we can send the message. Loop until the entire message has been
  // written.
  size_t bytes_written = 0;
  while (bytes_written < len) {
    // Try to write the entire remaining message.
    ssize_t rc = write(fd, message + bytes_written, len - bytes_written);

    // Did the write fail? If so, return an error.
    if (rc <= 0)
      return -1;

    // If there was no error, write returned the number of bytes written.
    bytes_written += rc;
  }

  return 0;
}

// Receive a message from a socket and return the message string (which must be
// freed later). Takes in a file descriptor.
char *receive_message(int fd) {
  // First try to read in the message length.
  size_t len;
  if (read(fd, &len, sizeof(size_t)) != sizeof(size_t)) {
    // Reading failed. Return an error.
    return NULL;
  }

  // Now make sure the message length is reasonable.
  if (len > MAX_MESSAGE_LENGTH) {
    errno = EINVAL;
    return NULL;
  }

  // Allocate space for the message.
  char *result = malloc(len + 1);

  // Try to read the message. Loop until the entire message has been read.
  size_t bytes_read = 0;
  while (bytes_read < len) {
    // Try to read the entire remaining message.
    ssize_t rc = read(fd, result + bytes_read, len - bytes_read);

    // Did the read fail? If so, return an error.
    if (rc <= 0) {
      free(result);
      return NULL;
    }

    // Update the number of bytes read.
    bytes_read += rc;
  }
  // End by putting a string end character at the end of the char array.
  result[len] = '\0';

  return result;
}

struct pollfd *our_fds; // Initialize (resizable) array of type struct pollfd.
nfds_t number_fd = 0;   // Holds number of pollfd structures to help iterate
                        // through clients/servers.

const char *username;

int server_socket_fd = 0; // Initializing fd of server socket.
// This function is run whenever the user hits enter after typing a message.
void input_callback(const char *message) {

  // If the user inputs :quit or :q, exit the ui and terminate program.
  if (strcmp(message, ":quit") == 0 || strcmp(message, ":q") == 0) {

    // if quitting, send message to show that someone is quitting to other peers.

    char messageArray[MAX_MESSAGE_LENGTH];
    char usernameArray[MAX_MESSAGE_LENGTH];

    // Store username and message in arrays to SEND.
    strncpy(usernameArray, username, MAX_MESSAGE_LENGTH);
    strncpy(messageArray, message, MAX_MESSAGE_LENGTH);

    // Traverse through usernames and messages and send to client fds stored in
    // our_fds.
    for (int i = 1; i < number_fd; i++) {
      send_message(our_fds[i].fd, usernameArray);
      send_message(our_fds[i].fd, messageArray);
    }
    // delete user's chat history file
      char userfilepath[100];
      getcwd(userfilepath, 100);
      strcat(userfilepath, "/ChatRecords/");
      strcat(userfilepath, username);
      strcat(userfilepath, ".txt");
      remove(userfilepath);

    ui_exit();
  } else {

    char messageArray[MAX_MESSAGE_LENGTH];
    char usernameArray[MAX_MESSAGE_LENGTH];

    // Store username and message in arrays to SEND.
    strncpy(usernameArray, username, MAX_MESSAGE_LENGTH);
    strncpy(messageArray, message, MAX_MESSAGE_LENGTH);

    sentiment = sentiment + sentiment_val(messageArray);


// if user is negative
    if (sentiment < 0){
      // send a string to note that they were being negative and thus kicked
      for (int i = 1; i < number_fd; i++){
      send_message(our_fds[i].fd, usernameArray);
      send_message(our_fds[i].fd, ":{})(");
      }
      //delete file
      char userfilepath[100];
      getcwd(userfilepath, 100);
      strcat(userfilepath, "/ChatRecords/");
      strcat(userfilepath, username);
      strcat(userfilepath, ".txt");
      remove(userfilepath);
      ui_exit();
    }

    // Traverse through usernames and messages and send to client fds stored in
    // our_fds.
    for (int i = 1; i < number_fd; i++) {
      send_message(our_fds[i].fd, usernameArray);
      send_message(our_fds[i].fd, messageArray);
    }
    // Display the Message.
    ui_display(username, message);

// if you receive a message, store it in text file for chat history.
    char userfilepath[100];
    getcwd(userfilepath, 100);
    strcat(userfilepath, "/ChatRecords/");
    strcat(userfilepath, username);
    strcat(userfilepath, ".txt");
    FILE *f = fopen(userfilepath, "a");

    fputs(message, f);
    fputs("\n", f);
    fclose(f);
  }
}

// A thread function that monitors server connections. Does not take in any 
// arguements.

void *server_connect(void *var) {
  
  // Loop endlessly.
  while (true) {

    int fd_status = poll(our_fds, number_fd, -1); // Check for status of fds.
    if (fd_status == -1) {
      perror("failed");
      exit(EXIT_FAILURE);
    }

    // Send message to clients after the first connection.
    for (int i = 1; i < number_fd;i++) { 
      // Check if client has a message. If not, move on.
      if (our_fds[i].revents ==0) { 
        continue;
      }
      if (our_fds[i].revents & POLLIN) { // CASE: Client has message.

        char *receive_username = receive_message(our_fds[i].fd); // Read
                                                                 // username.

        if (receive_username == NULL) { // Username error check.

          perror("\nusername read failed");

          close(our_fds[i].fd);

          our_fds[i].fd = -1; // Error on this file descriptor.

          continue;
        }
        char *receive_messg = receive_message(our_fds[i].fd); // Read message.
        if (receive_messg == NULL) { // Message error check.
          
          perror("\nmessage read failed");

          close(our_fds[i].fd);

          our_fds[i].fd = -1; // Mark error on that file discriptor.

          continue;
        }

        for (int j = 1; j < number_fd;
             j++) { // Send message and username to all clients.

          if (j != i) { // Don't send it to the peer that sent the message.
            send_message(our_fds[j].fd, receive_username);
            send_message(our_fds[j].fd, receive_messg);
          }
        }
          if (strcmp(receive_messg, ":q") == 0 || strcmp(receive_messg, ":quit") == 0){
            // if a user disconnected, remove  text file of that person
            ui_display(receive_username, "DISCONNECTED");
              char userfilepath[100];
             getcwd(userfilepath, 100);
             strcat(userfilepath, "/ChatRecords/");
             strcat(userfilepath, username);
             strcat(userfilepath, ".txt");
             remove(userfilepath);

              }else if(strcmp(receive_messg, ":{})(") == 0){
                // if they were kicked out, move file to kickedRecords
              ui_display(receive_username, "KICKED OUT - please look at 'kickedRecords' directory for more information");
              char userfilepath[100];
              getcwd(userfilepath, 100);
              strcat(userfilepath, "/ChatRecords/");
              strcat(userfilepath, username);
              strcat(userfilepath, ".txt");

             char newfilepath[100];
             getcwd(userfilepath, 100);
             strcat(userfilepath, "/kickedRecords/");
             strcat(userfilepath, username);
             strcat(userfilepath, ".txt");
             rename(userfilepath, newfilepath);
          } else{
            ui_display(receive_username,receive_messg); // Display the received 
                                                        // username and message.
          }
      }
    }

    // Connecting to a client.
    if (our_fds[0].revents & POLLIN) { // If bits returned in revents field
                                       // were POLLIN (i.e readable):

      // Accept clients and store fds.
      int server_fd = server_socket_accept(server_socket_fd);
      if (server_fd == -1) // Error check.
      {
        perror("accept failed");
        exit(EXIT_FAILURE);
      }
      // Show that Client has connected.
      ui_display("Client", "Connected!\n");

      // Creating space for new peer.
      number_fd++;
      // Creating memory of type pollfd to store fd of newly connected client.
      our_fds = realloc(our_fds,(sizeof(struct pollfd)) * number_fd);

      // Filling characteristics of new client fd.
      our_fds[number_fd - 1].fd = server_fd;
      our_fds[number_fd - 1].events = POLLIN;
    }
  }
}

int main(int argc, char **argv) {

  // Make sure the arguments include a username.
  if (argc != 2 && argc != 4) {
    fprintf(stderr, "Usage: %s <username> [<peer> <port number>]\n", argv[0]);
    exit(1);
  }

  // Save the given username.
  username = argv[1];

// create required directories
  mkdir("ChatRecords", S_IRWXU);

  mkdir("kickedRecords", S_IRWXU);

// setup sentiment value
  sentiment = 0.0;

//create vectors which we will work on
  SetupFunct();

// create chat history text file
char userfilepath[100];
getcwd(userfilepath, 100);
strcat(userfilepath, "/ChatRecords/");
strcat(userfilepath, username);
strcat(userfilepath, ".txt");
 FILE *f = fopen(userfilepath, "w+");
 fclose(f);

// send out how accurate current sentiment analysis is.
char* super = testingResults("twitter-test-set-c");


  // Open a socket.
  unsigned short port = 0;
  server_socket_fd = server_socket_open(&port);
  if (server_socket_fd == -1) {
    perror("Server socket was not opened");
    exit(EXIT_FAILURE);
  }
  // Allow the socket to accept connection requests.
  if (listen(server_socket_fd, 1)) {
    perror("listen failed");
    exit(EXIT_FAILURE);
  }
  //Save port and print to let users have port information.
  char my_port[10];
  snprintf(my_port, 10, "%d", port);

  // Initializing memory for struct pollfd* our_fd.
  our_fds = malloc(sizeof(struct pollfd));
  number_fd++; // Should be 1.
  // Storing starting connection characteristsics.
  our_fds[0].events = POLLIN;
  our_fds[0].fd = server_socket_fd;

  // Did the user specify a peer we should connect to?
  if (argc == 4) {
    // Unpack arguments.
    char *peer_hostname = argv[2];
    unsigned short peer_port = atoi(argv[3]);

    // Wait for a client to connect.
    int socket_fd = socket_connect(peer_hostname, peer_port);
    if (socket_fd == -1) {
      perror("Failed to connect");
      exit(EXIT_FAILURE);
    }
    char userfilepath[100];
    getcwd(userfilepath, 100);
    strcat(userfilepath, "/ChatRecords/");
    strcat(userfilepath, peer_hostname);
    strcat(userfilepath, ".txt");
    // make a new file for storing name
    FILE *f = fopen(userfilepath, "w+");
    fclose(f);

    // Creating space for new peer.
    number_fd++;
    // Creating memory of type pollfd to store fd of newly connected client.
    our_fds = realloc(our_fds,(sizeof(struct pollfd)) * number_fd);

    // Filling characteristics of client fd.
    our_fds[number_fd - 1].fd = socket_fd;
    our_fds[number_fd - 1].events = POLLIN;
  }
  //Initialize variable for thread id.
  pthread_t id;

  // Set up the user interface. The input_callback function will be called
  // each time the user hits enter to send a message.

  pthread_create(&id, NULL, server_connect, NULL);
  ui_init(input_callback);
  ui_display(" PORT", my_port); // Display port.
  ui_display("test", super);

  // Run the UI loop. This function only returns once we call ui_stop()
  // somewhere in the program.
  ui_run();

  return 0;
}

