#include "Response.hpp"
Response::Response() {
    sentLength = 0;
}

void Response::generateResponse(Request &request, std::vector<Server> const &servers) {
    //generateAutoindexPage
    // generateRedirectResponse //это предложил чатгпт, но пока не работает
    if (request.getMethod() == "GET" && request.getUrl() == "/redirect") {
        response = "HTTP/1.1 301 Moved Permanently\nLocation: http://localhost:8080/\n\n";
        return;
    }

    //generate generateCGIResponse
    if (isCGI(request.getUrl())) {
        generateCGIResponse(request, servers);
        return;
    }

    //generate HTML response
    handleRequest(request);
}

void Response::generateCGIResponse(Request &request, std::vector<Server> const &servers) {
    if (request.getMethod() == "POST" && request.getBody().empty()) {
        //generateErrorPage(config, 400);
        return;
    }
    const char *pythonInterpreter;
    std::cout << servers.size() << std::endl;
    for (size_t i = 0; i < servers.size(); i++) {
        if (servers[i].getHost() == this->ipAddress && servers[i].getPort() == this->port) {
            for (size_t j = 0; j < servers[i].getLocations().size(); j++) {
                    if (!servers[i].getLocations()[j].getCgiPass().empty()) {
                        pythonInterpreter = servers[i].getLocations()[j].getCgiPass().c_str();
                        break;
                    }
            }
        }
    }
    const char *pythonScriptPath = "/Users/gkhaishb/Desktop/webserv_project/Webserv/www/bin-cgi/what_day.py"; //захардкодил путь к скрипту, потом переделаю
    std::string pathInfo;
    std::string pathTranslated;
    std::string tmpBodyFile;
    //int hasBody = request.getMethod() == "POST" ? 1 : 0;
    std::map<std::string, std::string> env = request.getArgs();
    char **pythonEnv = new char *[2];
    std::map<std::string, std::string>::iterator it = env.begin();
    std::string tmp = it->first + "=" + it->second;
    pythonEnv[0] = strdup(tmp.c_str());
    //pythonEnv[0] = strdup("Number=3"); //цифра захаркодена, потом переделаю (это для второго скрипта какой день недели через n дней)
    pythonEnv[1] = NULL;
    ///generate args for execve
    char **pythonArgs = new char *[3];
    pythonArgs[0] = strdup(pythonInterpreter);
    pythonArgs[1] = strdup(pythonScriptPath);
    pythonArgs[2] = NULL;
    std::string tmpCGIFile = "/Users/gkhaishb/Desktop/webserv_project/Webserv/www/bin-cgi/tmpCGIFile"; //захардкодил путь к временному файлу, потом переделаю
    int fdCGIFile = open(tmpCGIFile.c_str(), O_RDWR | O_CREAT, 0666);
    if (fdCGIFile == -1) {
        perror("Ошибка при открытии файла");
        exit(1);
    }
    int pid = fork();
    if (!pid) {
        dup2(fdCGIFile, 1);
        if (execve(pythonInterpreter, pythonArgs, pythonEnv) == -1) { //переменная окружения пока не нужна
            perror("Ошибка при выполнении execve");
            exit(1);
        }
        close(fdCGIFile);
    }
    std::ifstream file( tmpCGIFile.c_str(), std::ios::binary);
    waitpid(pid, NULL, 0);
    if (file) {
        file.seekg(0, std::ios::end);
        std::streampos length = file.tellg();
        file.seekg(0, std::ios::beg);
        response.resize(length);
        file.read(&response[0], length);
        file.close();
    }
    remove(tmpCGIFile.c_str());
    delete[] pythonArgs;
}

bool Response::isCGI(std::string path) {
    ///check that last 3 symbols is .py
    if (path.size() > 2 && path.substr(path.size() - 3, 3) == ".py")
        return true;
    if (path.find(".py?") != std::string::npos)
        return true;
    return false;
}

void Response::handleGet(Request &request) {
    std::string url = request.getUrl();
    url.erase(0, 1);

    if (url.find(".jpg") != std::string::npos ||
        url.find(".png") != std::string::npos ||
        url.find(".svg") != std::string::npos ||
        url.find(".ico") != std::string::npos) {
        std::ifstream file(url.c_str(), std::ios::binary);
        if (!file.is_open() || file.fail()){
            return;
        }
        std::streampos len = file.seekg(0, std::ios::end).tellg();
        file.seekg(0, std::ios::beg);

        response = "HTTP/1.1 200 OK\n";
        if (url.find(".svg") != std::string::npos)
            response += "Content-Type: image/svg+xml";
        else
            response += "Content-Type: image/" + url.substr(url.find(".") + 1);
        response += "\nContent-Length: " + std::to_string(len) + "\n\n";
        std::string line;
        line.resize(len);
        file.read(&line[0], len);
        response += line + "\n\n";
        file.close();
        return;
    }
    std::ifstream file(url.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open() || file.fail()){
        response = "HTTP/1.1 404 Not Found\n\n";
        return;
    }
    response = "HTTP/1.1 200 OK\n\n";
    std::string line;
    std::getline(file, line, '\0');
    response += line;
    file.close();
}

void Response::handlePost(Request &request) {
    (void)request;
}


void Response::handleRequest(Request &request) {

    std::cout << request.request << std::endl;

    if(request.getUrl() == "/upload") {

        std::string formData = request.request.substr(request.request.find("\r\n\r\n") + 4);

        std::istringstream stream(formData);

        std::cout << formData << std::endl;

        std::string line;
        std::ofstream destFile;
        destFile.open("upload.txt", std::ios::binary | std::ios::app);

        while (std::getline(stream, line)) {
            destFile << line << std::endl;
        }

        destFile.close();
    }

    if (request.getMethod() == "GET") {
        handleGet(request);
    } else if (request.getMethod() == "POST") {
        handlePost(request);
    } else if (request.getMethod() == "DELETE") {
        std::cout << request.getMethod() << std::endl;
    } else {
        std::cout << "ERROR" << std::endl;
    }
}

void Response::createResponse(Request &request) {
    (void)request;
}

std::string Response::getIpAddress() const {
    return ipAddress;
}

void Response::setIpAddress(std::string ipAddress) {
    this->ipAddress = ipAddress;
}

int Response::getPort() const {
    return port;
}

void Response::setPort(int port) {
    this->port = port;
}
