/* IPK 2013/2014
 * Projekt c.1 - FTP klient
 * Autor: Jan Krocil
 *        xkroci02@stud.fit.vutbr.cz
 * Datum: 9.3.2014
 */

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>


#define FTP_OK 0
#define FTP_ERROR 1
#define STR_BUFF_SIZE 4096


 // globals
int DEBUG = 0;

struct timeval TIMEOUT = {
  .tv_sec = 10,
  .tv_usec = 0
};
// --------

struct parsed_url {
  // [ftp://[user:password@]]host[:21][/path/to][/]
  char username[128]; // user
  char password[128]; // password
  char hostname[256]; // host
  int  port;          // 21
  char path[256];     // /path/to/
};


struct ftp_connection {
  struct sockaddr_in sock_in;
  struct sockaddr_in pasv_sock_in;
  int socket;
};


ssize_t read_line(int fildes, char *buf, ssize_t buff_size) {
  ssize_t b_read = 0;
  ssize_t b_read_all = 0;
  while ((b_read = read(fildes, buf + b_read_all, 1)) == 1) {
    b_read_all++;
    if (b_read_all >= buff_size)
      return -1;
    if (buf[b_read_all-1] == '\n')
      break;
  }
  buf[b_read_all] = '\0';
  if (b_read < 0)
    return b_read;
  return b_read_all;
}


ssize_t read_resp(int fildes, char *buf, size_t buff_size) {
  ssize_t b_read = 0;
  int is_last_line;
  do {
    if ((b_read = read_line(fildes, buf, buff_size)) < 0)
      return b_read;
    buf[b_read] = '\0';
    if (DEBUG) printf("%s", buf);
    is_last_line = (strlen(buf) > 3) && isdigit(buf[0]) && isdigit(buf[1]) && isdigit(buf[2]) && (buf[3] == ' ');
  } while (!is_last_line);
  return b_read;
}


int parse_numbers(char *str, int int_array[], int arr_size) {
  int num_index = 0;
  char *p_index = str;

  while (*p_index) {
    if (isdigit(*p_index)) {
      if (arr_size <= num_index)
        return -1;
      int_array[num_index] = strtoul(p_index, &p_index, 10);
      num_index++;
    }
    else
      p_index++;
  }

  return num_index;
}


int write_and_read(int socket, char *str_buffer) {
  if (DEBUG) printf("%s", str_buffer);
  if (write(socket, str_buffer, strlen(str_buffer)) < 0)
    return 1;
  if (read_resp(socket, str_buffer, STR_BUFF_SIZE) < 0)
    return 1;

  return 0;
}


int ftp_perror() {
  if (errno)
    perror(NULL);
  return FTP_ERROR;
}


int ftp_parse_url(char *raw_url, struct parsed_url *p_url) {
  char str_buff[STR_BUFF_SIZE] = "";
  char *c = raw_url, *str_i = NULL;

  if (strlen(raw_url) >= STR_BUFF_SIZE)
    return FTP_ERROR;

  if (strncmp(raw_url, "ftp://", 6) == 0) {
    // drop scheme
    c += 6;

    // if @ in url, username:password follows
    char *search_c = c;
    int user_pass = 0;
    while (*search_c++) {
      if (*search_c == '@')
        user_pass = 1;
    }

    // username:password@
    if (user_pass) {
      str_i  = p_url->username;
      while(*c && (*c != ':'))
        *str_i++ = *c++;
      *str_i = '\0';
      if (*c == ':')
        c++;

      str_i  = p_url->password;
      while(*c && (*c != '@'))
        *str_i++ = *c++;
      *str_i = '\0';

      if (*c == '@')
        c++;
    }
  }
  
  // hostname
  str_i = p_url->hostname;
  while(*c) {
    if (*c == ':' || *c == '/')
      break;
    *str_i++ = *c++;
  }
  *str_i = '\0';
  if (strlen(p_url->hostname) == 0)
    return FTP_ERROR;

  if (*c == ':')
      c++;

  // port
  str_i = str_buff;
  while(*c && (*c != '/')) {
    if (!isdigit(*c))
      return FTP_ERROR;
    *str_i++ = *c++;
  }
  *str_i = '\0';
  if (strlen(str_buff) == 0)
    p_url->port = 21;
  else
    p_url->port = atoi(str_buff);

  // path
  str_i = p_url->path;
  while(*c)
    *str_i++ = *c++;
  *str_i = '\0';

  if (DEBUG) {
    printf("Parsed URL:\n"\
           "User: '%s'\n"\
           "Pass: '%s'\n"\
           "Host: '%s'\n"\
           "Port: '%d'\n"\
           "Path: '%s'\n"\
           "\n"\
           "Communication:\n",
           p_url->username, p_url->password, p_url->hostname, p_url->port, p_url->path);
  }

  return FTP_OK;
}


int ftp_connect(char *hostname, int port, struct ftp_connection *conn) {
  char str_buff[STR_BUFF_SIZE];

  if ((conn->socket = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    return ftp_perror();

  if (setsockopt (conn->socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&TIMEOUT, sizeof(TIMEOUT)) < 0)
    return ftp_perror();

  conn->sock_in.sin_family = PF_INET;
  conn->pasv_sock_in.sin_family = PF_INET;
  conn->sock_in.sin_port = htons(port);
  conn->pasv_sock_in.sin_port = 0;

  struct hostent *host_e = NULL;
  if ((host_e = gethostbyname(hostname)) == NULL)
    return ftp_perror();
  memcpy(&(conn->sock_in.sin_addr), host_e->h_addr_list[0], host_e->h_length);
  memcpy(&(conn->pasv_sock_in.sin_addr), host_e->h_addr_list[0], host_e->h_length);

  if (connect(conn->socket, (struct sockaddr *)&(conn->sock_in), sizeof(conn->sock_in)) < 0)
    return ftp_perror();
  if (read_resp(conn->socket, str_buff, STR_BUFF_SIZE) < 0)
    return ftp_perror();
  if (str_buff[0] != '2') // 220
    return ftp_perror();

  return FTP_OK;
}


int ftp_login(char *username, char *password, struct ftp_connection *conn) {
  char str_buff[STR_BUFF_SIZE] = "";

  if (strlen(username) == 0) {
    username = "anonymous";
    password = "";
  }

  sprintf(str_buff, "USER %s\r\n", username);

  if (write_and_read(conn->socket, str_buff) != 0)
    return ftp_perror();
  if (str_buff[0] != '3') // 331
    return ftp_perror();

  sprintf(str_buff, "PASS %s\r\n", password);

  if (write_and_read(conn->socket, str_buff) != 0)
    return ftp_perror();
  if (str_buff[0] != '2') // 230
    return ftp_perror();

  return FTP_OK;
}


int ftp_set_passive_mode(struct ftp_connection *conn) {
  char str_buff[STR_BUFF_SIZE] = "";
  strcpy(str_buff, "PASV\r\n");

  if (write_and_read(conn->socket, str_buff) != 0)
    return ftp_perror();
  if (str_buff[0] != '2') // 227
    return ftp_perror();

  int numbers[7];
  if (parse_numbers(str_buff, numbers, 7) != 7)
    return ftp_perror();

  int pasv_port = (numbers[5] * 256) + numbers[6];
  conn->pasv_sock_in.sin_port = htons(pasv_port);

  return FTP_OK;
}


int ftp_list(char *path, struct ftp_connection *conn) {
  char str_buff[STR_BUFF_SIZE] = "";
  int passive_socket;
  int read_ret = 0;

  sprintf(str_buff, "LIST %s\r\n", path);

  if (DEBUG) printf("%s", str_buff);
  if (write(conn->socket, str_buff, strlen(str_buff)) < 0)
    return ftp_perror();

  if ((passive_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    return ftp_perror();
   
  if (setsockopt (passive_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&TIMEOUT, sizeof(TIMEOUT)) < 0)
    return ftp_perror();

  if (DEBUG) printf("Connecting to port %d\n", ntohs(conn->pasv_sock_in.sin_port));
  if (connect(passive_socket, (struct sockaddr *)&(conn->pasv_sock_in), sizeof(conn->pasv_sock_in)) < 0)
    return ftp_perror();

  if (read_resp(conn->socket, str_buff, STR_BUFF_SIZE) < 0)
    return ftp_perror();
  if (str_buff[0] != '1') // 150
    return ftp_perror();

  while ((read_ret = read_line(passive_socket, str_buff, STR_BUFF_SIZE)) > 0) {
    if (printf("%s", str_buff) < 1)
      return ftp_perror();
  }
  if ((close(passive_socket) < 0) || (read_ret < 0))
    return ftp_perror();

  if (read_resp(conn->socket, str_buff, STR_BUFF_SIZE) < 0)
    return ftp_perror();
  if (str_buff[0] != '2') // 226
    return ftp_perror();

  return FTP_OK;
}


int ftp_disconnect(struct ftp_connection *conn) {
  char str_buff[STR_BUFF_SIZE] = "";
  strcpy(str_buff, "QUIT\r\n");

  if (write_and_read(conn->socket, str_buff) != 0)
    return ftp_perror();
  if (str_buff[0] != '2') // 221
    return ftp_perror();
  if (close(conn->socket) < 0)
    return ftp_perror();

  return FTP_OK;
}


int main(int argc, char *argv[]) {
  int status = FTP_OK;
  struct parsed_url p_url = {{0}};
  struct ftp_connection conn = {{0}};

  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  if (argc == 3 && strcmp(argv[2], "--debug") == 0)
    DEBUG = 1;

  if ((argc != 2) && (DEBUG != 1)) {
    status = FTP_ERROR;
    fprintf(stderr, "Error: Invalid argument.\n");
    printf("%s URL [--debug]\n    URL: [ftp://[user:password@]]host[:21][/path][/]\n\n", argv[0]);
    goto quit;
  }

  if ((status = ftp_parse_url(argv[1], &p_url)) != FTP_OK) {
    fprintf(stderr, "Error: Failed to parse given URL.\n");
    goto quit;
  }
  
  if ((status = ftp_connect(p_url.hostname, p_url.port, &conn)) != FTP_OK) {
    fprintf(stderr, "Error: Failed to connect to FTP server.\n");
    goto quit;
  }

  if ((status = ftp_login(p_url.username, p_url.password, &conn)) != FTP_OK) {
    fprintf(stderr, "Error: Failed to login to FTP server.\n");
    goto disconnect;
  }
  
  if ((status = ftp_set_passive_mode(&conn)) != FTP_OK) {
    fprintf(stderr, "Error: Failed to set mode to passive.\n");
    goto disconnect;
  }
  
  if ((status = ftp_list(p_url.path, &conn) != FTP_OK)) {
    fprintf(stderr, "Error: Failed to list files in given path.\n");
    goto disconnect;
  }

  disconnect:
  if (ftp_disconnect(&conn) != FTP_OK) {
    fprintf(stderr, "Error: Failed to disconnect from FTP server.\n");
    status = FTP_ERROR;  
  }

  quit:
  if (status != FTP_OK)
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
} 
