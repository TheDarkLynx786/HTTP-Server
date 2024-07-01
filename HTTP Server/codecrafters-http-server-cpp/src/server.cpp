#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <fstream>


std::vector<std::string> split(const std::string str, const char delim) {
  std::vector<std::string> output;
  std::string elem = "";
  for(int i = 0; i < str.length(); i++) {
    if(delim == str.at(i)) {
      if(elem.length() > 0) output.push_back(elem);
      elem = "";
      continue;
    } else {
      elem = elem + str.at(i);
      continue;
    }
  }
  
  if(elem.length() > 0) output.push_back(elem);
  if(output.size()==0) output.push_back(str);
  return output;
}

void GetFile(std::vector<std::string> paths, std::string directory, std::string &response) {
  std::string fileDir = directory + paths.back();

  std::fstream file;

  file.open(fileDir, std::ios::in | std::ios::binary);

  //Check File Existence
  if(!file.is_open()) {
    response = "HTTP/1.1 404 Not Found\r\n\r\n"; 
    std::cout << "File Does Not Exist" << std::endl;
    return;
  } //DNE code

  //File Length
  file.seekg(0, file.end);
  int length = file.tellg();

  //Reset 
  file.seekg(0, file.beg);

  //Get Data
  std::string data = "";
  while(file) {
    data += file.get();
  }

  //Close
  file.close();

  //Response Set
  response = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " + std::to_string(length) + "\r\n\r\n" + data;
  
}

void WriteFile(std::vector<std::string> paths, std::string directory, std::string &response, std::string request) {
  std::string fileDir = directory + paths.back();

  //Parsing
  
  //Get Content Length
  size_t indexUncast = request.find("Content-Length:");
  int index = 0;
  if(indexUncast != std::string::npos) index = static_cast<int>(indexUncast);
  index += 16;

  std::string sizeStr = "";
  char stop = 'l';
  while(stop != '\r') {
    sizeStr += request.at(index);
    index++;
    stop = request.at(index);
  }
  
  int bodySize = stoi(sizeStr);

  //Get Content
  indexUncast = request.rfind("\r\n\r\n");
  index = 5;
  if(indexUncast != std::string::npos) index = static_cast<int>(indexUncast);
  index += 4; // "\n" counts as 1 character: \r\n\r\n == 4 characters

  std::string reqBody = "";

  for(int i = 0; i < bodySize; i++) {
    reqBody += request.at(index + i);
  }
  
  //File Object
  std::fstream file;
  
  //Add Contents
  file.open(fileDir, std::ios::out | std::ios::binary);
  file << reqBody;
  file.close();
  

  //Response Set
  response = "HTTP/1.1 201 Created\r\nContent-Type: application/octet-stream\r\nContent-Length: " + std::to_string(bodySize) + "\r\n\r\n" + reqBody;

}

void RecvAndProcess(int clientSocket, std::string directory) {
  //Recieve
  std::string request(1024, '\0');
  recv(clientSocket, (void*) &request[0], request.size(), 0);
  
  //Process Response
  std::string response = "";
  std::vector<std::string> reqContents = split(request, ' '); //From "GET /echo/banana HTTP/1.1\r\nHost: localhost:4221\r\n\r\n" --> "/echo/banana"
  std::string path = reqContents.at(1);
  std::vector<std::string> paths = split(path, '/');

  
  if(path == "/") {
    response = "HTTP/1.1 200 OK\r\n\r\n";
  } else if(paths.at(0) == "echo") {
    response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(paths.at(1).length()) + "\r\n\r\n" + paths.at(1);
  } else if(paths.at(0) == "user-agent") {
    
    for(int i = 0; i < reqContents.size(); i++) {
      
      if(reqContents.at(i).find("User-Agent:") != std::string::npos) {
        std::string userAgent = "";
        for(int l = 0; l < reqContents.at(i+1).length(); l++) {        
          if (reqContents.at(i+1).at(l) != '\r') {
            userAgent += reqContents.at(i+1).at(l);
          } else {
            break;
          }
        }
        response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(userAgent.length()) + "\r\n\r\n" + userAgent;
        break;
      }

    }
  
  } else if(paths.at(0) == "files" && reqContents.at(0) == "GET") {
    GetFile(paths, directory, response);
  } else if(paths.at(0) == "files" && reqContents.at(0) == "POST") {
    WriteFile(paths, directory, response, request);
  } else {
    response = "HTTP/1.1 404 Not Found\r\n\r\n";
  }

  //Send
  send(clientSocket, response.c_str(), response.length(), 0);

}


int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  //Grab Flags and Stuff
  std::string directory = "";
  if(argc >= 3) {
    if(strcmp(argv[1], "--directory") == 0) {
      directory = argv[2];
    }
  }

  std::cout << directory << std::endl;

  // You can use print statements as follows for debugging, they'll be visible when running tests.
  // std::cout << "Logs from your program will appear here!\n";

  // Uncomment this block to pass the first stage
  
  //Socket
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  //Set Socket Options (?)
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221);
  
  //Bind
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 4221\n";
    return 1;
  }
  
  //Listen
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  
  std::cout << "Waiting for a client to connect...\n";
  
  //Accept
  while(true) {  
    int clientSocket = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    std::cout << "Client Connected." << std::endl;
    std::thread th(RecvAndProcess, clientSocket, directory);
    th.detach();
  }
  
  //Close Server
  close(server_fd);

  return 0;
}
