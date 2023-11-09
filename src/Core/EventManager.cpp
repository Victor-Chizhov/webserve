#include "../../include/EventManager.hpp"
#include <iostream>
#include <ratio>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fstream>
#include "../request/Request.hpp"
#include "../../include/Parser.hpp"
#include "../../include/ServerSocket.hpp"

void handleRequest(std::string buffer, int newsockfd)
{

		// Print message from client
		std::cout << buffer << std::endl;

		Request request(buffer);
		std::cout << request.getUrl() << std::endl;
		std::string url = request.getUrl();
		url.erase(0, 1);

		if (url.find(".jpg") != std::string::npos ||
				url.find(".png") != std::string::npos ||
				url.find(".svg") != std::string::npos ||
				url.find(".ico") != std::string::npos) {
			std::cout << "IMAGE" << std::endl;
			std::ifstream file(url.c_str(), std::ios::binary);
			if (!file.is_open() || file.fail()){
				std::cout << url << std::endl;
				close(newsockfd);
				//continue;
			}
			// get length of file:
			std::streampos len = file.seekg(0, std::ios::end).tellg();
			file.seekg(0, std::ios::beg);

			std::string response;

			response = "HTTP/1.1 200 OK\n";
			if (url.find(".svg") != std::string::npos)
				response += "Content-Type: image/svg+xml";
			else {
				response += "Content-Type: image/";
				response += url.substr(url.find(".") + 1);
			}
			response += "\n";
			response += "Content-Length: ";
			response += std::to_string(len);
			response += "\n\n";
			std::string line;
			line.resize(len);
			file.read(&line[0], len);
			response += line;
			response += "\n\n";
			std::cout << "len: " << response.length() << std::endl;
			send(newsockfd, response.c_str(), response.length(), 0);
			file.close();
			close(newsockfd);
			//continue;
		}
		std::ifstream file(url.c_str(), std::ios::in | std::ios::binary);
		std::string response;
		if (!file.is_open() || file.fail()){
			response = "HTTP/1.1 404 Not Found\n\n";
			send(newsockfd, response.c_str(), response.length(), 0);
			close(newsockfd);
			//continue;
		}
		response = "HTTP/1.1 200 OK\n\n";
		std::string line;
		while (std::getline(file, line)){
			response += line;
		}
		file.close();
		send(newsockfd, response.c_str(), response.length(), 0);

		// Close sockets
		close(newsockfd);
}

EventManager::EventManager() : maxSocket(0) {
    FD_ZERO(&readSet);
}

EventManager::~EventManager() {
    // Ничего особенного для деструктора, но можно добавить необходимую логику
}

// Метод для добавления клиентского сокета в event-менеджер
void EventManager::addServerSocket(int ServerSocket) {
    serverSockets.push_back(ServerSocket);
    FD_SET(ServerSocket, &readSet);

    if (ServerSocket > maxSocket) {
        maxSocket = ServerSocket;
    }
}

void EventManager::addClientSocket(int clientSocket) {
    // Добавляем клиентский сокет в список отслеживаемых сокетов
    clientSockets.push_back(Client(clientSocket));

    // Добавляем клиентский сокет в множество для использования в select
    FD_SET(clientSocket, &readSet);

    // Обновляем максимальный дескриптор, если необходимо
    if (clientSocket > maxSocket) {
        maxSocket = clientSocket;
    }
}

// Метод для ожидания событий и их обработки
void EventManager::waitAndHandleEvents() {
	
    while (true) {
        fd_set tempReadSet = readSet;
        int activity = select(maxSocket + 1, &tempReadSet, NULL, NULL, NULL);

        if (activity == -1) {
            perror("Error in select");
            break;
        }

        if (activity > 0) {
			std::list<Client>::iterator itBegin = clientSockets.begin();
			std::list<Client>::iterator itEnd = clientSockets.end();
            for (std::list<Client>::iterator it = itBegin; it != itEnd; ++it) {
                int currentSocket = (*it).getClientSocket();
                if (FD_ISSET(serverSockets[0], &tempReadSet)) {
                        // Если событие на слушающем сокете, это новое подключение
						struct sockaddr_in currentClientAddr = (*it).getStruct();
						socklen_t clientAddrLen = sizeof(currentClientAddr);
                        int clientSocket = accept(serverSockets[0], (struct sockaddr*) &currentClientAddr, &clientAddrLen);
                        if (clientSocket == -1) {
                            perror("Error accepting connection");
                            // Обработка ошибки при принятии нового соединения
                        } else {
                            std::cout << "New connection accepted, socket: " << clientSocket << std::endl;
                            addClientSocket(clientSocket);
                        }
				} else {
					// Обработка других событий, например, чтение данных из клиентского сокета
					char buffer[1024];
					ssize_t bytesRead = recv(currentSocket, buffer, sizeof(buffer), 0);
					if (bytesRead <= 0) {
						std::cout << "Connection closed or error on socket: " << currentSocket << std::endl;
						close(currentSocket);
						FD_CLR(currentSocket, &readSet);
						clientSockets.erase(it);
						--it;
					} else {
						std::cout << "Received data from socket " << currentSocket << ": " << buffer << std::endl;
						std::string httpRequest(buffer, bytesRead);
						handleRequest(httpRequest, currentSocket);
					}
				}
			}
		}
	}
}

// Если select() возвращает положительное значение, идем по всем добавленным сокетам, проверяем, какие из них имеют активные события (например, данные для чтения), и обрабатываем их соответственно.

//Если при чтении обнаруживается, что соединение закрыто или произошла ошибка, сокет закрывается, удаляется из fd_set и удаляется из вектора clientSockets.