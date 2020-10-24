#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/stat.h>

#define IP "0.0.0.0"
#define SERVER_PORT 80

static int debug = 1;

int GetLine(int sockfd, char* buf, int size);
void DoHttpRequest(int client_sock);
void DoHttpResponse(int client_sock, char* path);

//异常处理
void BadRequest(int clinet_sock); //400
void Unimplemente(int client_sock); //501
void NotFound(int client_sock);  //404

int main() {
	int server_sock;
	int ret;
	struct sockaddr_in server_addr;
	//create server socket
	server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == server_sock) {
	    fprintf(stderr, "create socket error, reason: %s\n", strerror(errno));
        exit(1);
    }

	//clear and reset server ip and port
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;           //select protocl IPV4
	inet_pton(AF_INET, IP, &server_addr.sin_addr.s_addr);
	//server_addr.sin_addr.s_addr = htonl("1.1.1.1");  //listen all local ip
	server_addr.sin_port = htons(SERVER_PORT);

	//bind server address to socket
	ret = bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (-1 == ret) {
		fprintf(stderr, "bind error, reason: %s\n", strerror(errno));
		close(server_sock);
		exit(1);
    }
        
	//listen socket
	ret = listen(server_sock, 128); //128 is the number of client
		
    if (-1 == ret) {
        fprintf(stderr, "listen error, reason: %s\n", strerror(errno));
        close(server_sock);
	    exit(1);
    }


	int done = 1;
	while(done) {
		struct sockaddr_in client_addr;
		int client_sock;
		char client_ip[64];

		socklen_t client_addr_len;
		client_addr_len = sizeof(client_addr);
		client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len);

		printf("Client ip: %s\t port: %d\n",
				inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_ip, sizeof(client_ip)),
				ntohs(client_addr.sin_port));
		
		DoHttpRequest(client_sock);

		close(client_sock);
	}

	return 0;
}

//获取请求中的一行
int GetLine(int sockfd, char* buf, int size) {
	int count = 0;
	char ch = '\0';
	int len = 0;
    
	while(count < size-1 && ch != '\n') {
		len = read(sockfd, &ch, 1);
		if (1 == len) {
			if ('\r' == ch) {
				continue;
			} else if ('\n' == ch) {
				buf[count] = '\0';
				break;
			}
			buf[count] = ch;
			++count;
		} else if (-1 == len) { 
			perror("read failed.");
			break;
		} else {   //client socket close
			fprintf(stderr, "client close.\n");
			break;
		}
 	}
	return count;
}


//解析请求
void DoHttpRequest(int client_sock) {
	int len;

	char buf[256];
	char request_method[16];
	char url[256];

	len = GetLine(client_sock, buf, sizeof(buf));
	if (len > 0) {
		int i = 0, j = 0;	
		while (i < sizeof(request_method) - 1 && i < len && !isspace(buf[i])) {
			request_method[i] = buf[i];
			++i;
		}
		request_method[i] = '\0';
		
		//Get Method
		if (strncasecmp(request_method, "Get", i) == 0) {
			printf("request method: %s\n", request_method);
			
			while(isspace(buf[i])) ++i;	
			while (j < sizeof(url) - 1 && i < len && !isspace(buf[i])) {
				url[j] = buf[i];
				++i; ++j;
			}

			url[j] = 0;
			printf("url: %s\n", url);

			//obtain file path
			char path[256];
			sprintf(path, "./html_docs%s", url);
			if (path[strlen(path)-1] == '/') {
				strcat(path, "index.html");
			}
			printf("path: %s\n", path);

			//read other data
			do {
				len = GetLine(client_sock, buf, sizeof(buf));
				printf("read line: %s\n", buf);
			} while(0 < len);

			//check whether file is exit
			struct stat st;
			int res = stat(path, &st);
			if (stat(path, &st) == -1) {  //file does not exit
				NotFound(client_sock);
			}  else {
				//check whether the path is directory
				if (S_ISDIR(st.st_mode)) {
					strcat(path, "/index.html");
					printf("path: %s\n", path);
				}
				DoHttpResponse(client_sock, path);
			}
		} else {
			printf("request method: %s\n", request_method);
			//read other data
			do {
				len = GetLine(client_sock, buf, sizeof(buf));
				printf("read line: %s\n", buf);
			} while(0 < len);

			Unimplemente(client_sock);
		}
			
	} else  {
		BadRequest(client_sock);
	}


}

//响应请求
void DoHttpResponse(int client_sock, char* path) {
	const char* main_header = "HTTP/1.0 200 OK\r\n Server: MiniHttp Server\r\nContent-Type: text html\r\nConnection: Close\r\n";
		FILE* file;
	char welcome_content[1024] = {0};
	char line_content[256];
	if (!(file=fopen(path,"r")))
	{
       printf("Error in open file!\n");
	   return ;
   	}
	while (!feof(file)) {
		fgets(line_content, sizeof(line_content), file);
		strcat(welcome_content, line_content);
	}

	fclose(file);
	//const char* welcome_content = "<html lang=\"zh-CN\">\n <head>\n<meta content=\"text/html; charset=utf-8\" http-equiv=\"Content-Type\">\n <title> This is a test</title></head>\
	<body><h2> 大家好，这是一个MINI_HTTP</h2><br/><br/><form action=\"commit\" method=\"post\"> 尊姓大名： <input type = \"text\", name = \"name\"><input type=\"submit\" value=\"提交\"/> </body>";

	char send_buf[64];
	int welcome_content_len = strlen(welcome_content);
	int len = write(client_sock, main_header, strlen(main_header));
	if (debug)
		fprintf(stdout, ".... do http response...\n");

	len = snprintf(send_buf, 64, "Content-Length: %d\r\n\r\n", welcome_content_len);
	len = write(client_sock, send_buf, len);
	len = write(client_sock, welcome_content, welcome_content_len);
}


void BadRequest(int client_sock) {
	const char* reply = "HTTP/1.0 400 BAD REQUEST\r\nContent-Type: text/html\r\n<html>\r\n<head>\r\n<title>BAD REQUEST<</title>\r\n</head>\r\n<body>\r\n<p>Your browser sent a bad request</p>\r\n</body>\r\n</html>";

	int len = write(client_sock, reply, strlen(reply));
	if (len <= 0) {
		fprintf(stderr, "send reply failed, reason: %s\n", strerror(errno));
	}
}

void Unimplemente(int client_sock) {
	const char* reply = "HTTP/1.0 501 Method Not Implemented\r\nContent-Type: text/html\r\n<html>\r\n<head>\r\n<title>Method Not Implemented<</title>\r\n</head>\r\n<body>\r\n<p>HTTP request method not supported</p>\r\n</body>\r\n</html>";

	int len = write(client_sock, reply, strlen(reply));
	if (len <= 0) {
		fprintf(stderr, "send reply failed, reason: %s\n", strerror(errno));
	}

}

void NotFound(int client_sock) {
	const char* main_header = "HTTP/1.0 404 NOT FOUND\r\n Server: MiniHttp Server\r\nContent-Type: text html\r\nConnection: Close\r\n";
	
	const char* welcome_content = "<html>\r\n<head>\r\n<title>NOT FOUND</title>\r\n</head>\r\n<body>\r\n<p>The file does not found</p>\r\n</body>\r\n</html>";

	char send_buf[64];
	int welcome_content_len = strlen(welcome_content);
	int len = write(client_sock, main_header, strlen(main_header));
	if (debug)
		fprintf(stdout, ".... do http response...\n");

	len = snprintf(send_buf, 64, "Content-Length: %d\r\n\r\n", welcome_content_len);
	len = write(client_sock, send_buf, len);
	len = write(client_sock, welcome_content, welcome_content_len);

}