// Simple 2-player Rock Paper Scissors client.
// Build on Windows: g++ client.cpp -std=c++17 -O2 -o client.exe -lws2_32
// Build on Linux/macOS: g++ client.cpp -std=c++17 -O2 -o client

#include <cctype>
#include <cstring>
#include <iostream>
#include <string>

#ifdef _WIN32
    #define NOMINMAX
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using Socket = SOCKET;
    const Socket BAD_SOCKET = INVALID_SOCKET;
    void closeSocket(Socket s) { closesocket(s); }
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
    using Socket = int;
    const Socket BAD_SOCKET = -1;
    void closeSocket(Socket s) { close(s); }
#endif

constexpr int PORT = 54000;

const char* GREEN = "\033[32m";
const char* CYAN = "\033[36m";
const char* YELLOW = "\033[33m";
const char* RED = "\033[31m";
const char* RESET = "\033[0m";

#ifdef _WIN32
bool startSockets() {
    WSADATA wsa{};
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
}
void stopSockets() { WSACleanup(); }
#else
bool startSockets() { return true; }
void stopSockets() {}
#endif

bool sendLine(Socket s, const std::string& text) {
    std::string data = text + "\n";
    size_t sent = 0;
    while (sent < data.size()) {
        int n = send(s, data.data() + sent, static_cast<int>(data.size() - sent), 0);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool recvLine(Socket s, std::string& out) {
    out.clear();
    char c;
    while (true) {
        int n = recv(s, &c, 1, 0);
        if (n <= 0) return false;
        if (c == '\n') break;
        if (c != '\r') out += c;
        if (out.size() > 1000) return false;
    }
    return true;
}

std::string askMove() {
    while (true) {
        std::cout << YELLOW << "Your move [R]ock [P]aper [S]cissors: " << RESET;
        std::string input;
        std::getline(std::cin, input);
        if (!input.empty()) {
            char c = static_cast<char>(std::toupper(static_cast<unsigned char>(input[0])));
            if (c == 'R' || c == 'P' || c == 'S') return std::string(1, c);
        }
        std::cout << RED << "Please enter R, P, or S.\n" << RESET;
    }
}

std::string moveName(char c) {
    if (c == 'R') return "Rock";
    if (c == 'P') return "Paper";
    if (c == 'S') return "Scissors";
    return "Invalid";
}

int main() {
    if (!startSockets()) {
        std::cerr << "Could not start networking.\n";
        return 1;
    }

    std::string ip;
    std::string name;
    std::cout << CYAN;
    std::cout << "======================================\n";
    std::cout << "       ROCK PAPER SCISSORS CLIENT     \n";
    std::cout << "======================================\n" << RESET;
    std::cout << "Server IP [127.0.0.1]: ";
    std::getline(std::cin, ip);
    if (ip.empty()) ip = "127.0.0.1";
    std::cout << "Your name: ";
    std::getline(std::cin, name);
    if (name.empty()) name = "Player";

    Socket sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == BAD_SOCKET) {
        std::cerr << "Could not create socket.\n";
        stopSockets();
        return 1;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    if (inet_pton(AF_INET, ip.c_str(), &server.sin_addr) <= 0) {
        std::cerr << "Invalid server IP.\n";
        closeSocket(sock);
        stopSockets();
        return 1;
    }

    std::cout << "Connecting to " << ip << ":" << PORT << "...\n";
    if (connect(sock, reinterpret_cast<sockaddr*>(&server), sizeof(server)) != 0) {
        std::cerr << RED << "Connection failed. Start the server first.\n" << RESET;
        closeSocket(sock);
        stopSockets();
        return 1;
    }

    std::string msg;
    if (!recvLine(sock, msg) || msg.rfind("PLAYER ", 0) != 0) {
        std::cerr << "Unexpected server response.\n";
        closeSocket(sock);
        stopSockets();
        return 1;
    }
    int playerNumber = std::stoi(msg.substr(7));

    if (!recvLine(sock, msg) || msg != "NAME" || !sendLine(sock, name)) {
        std::cerr << "Login failed.\n";
        closeSocket(sock);
        stopSockets();
        return 1;
    }

    if (!recvLine(sock, msg) || msg.rfind("START ", 0) != 0) {
        std::cerr << "Waiting for second player...\n";
    } else {
        std::cout << GREEN << "\nBoth players are connected!\n" << RESET;
    }

    std::cout << "You are Player " << playerNumber << ".\n";

    while (recvLine(sock, msg)) {
        if (msg == "MOVE") {
            std::string move = askMove();
            if (!sendLine(sock, move)) break;
        } else if (msg.rfind("RESULT ", 0) == 0) {
            char move1 = msg[7];
            char move2 = msg[9];
            char myMove = (playerNumber == 1) ? move1 : move2;
            char otherMove = (playerNumber == 1) ? move2 : move1;
            int winner = std::stoi(msg.substr(11, 1));
            size_t p1 = msg.find(' ', 13);
            int score1 = std::stoi(msg.substr(13, p1 - 13));
            int score2 = std::stoi(msg.substr(p1 + 1));
            int myScore = (playerNumber == 1) ? score1 : score2;
            int opponentScore = (playerNumber == 1) ? score2 : score1;

            std::cout << "\n" << CYAN << "========== RESULT ==========\n" << RESET;
            std::cout << "You:       " << moveName(myMove) << "\n";
            std::cout << "Opponent:  " << moveName(otherMove) << "\n";
            if (winner == 0)
                std::cout << YELLOW << "Draw!" << RESET << "\n";
            else if (winner == playerNumber)
                std::cout << GREEN << "You win the round!" << RESET << "\n";
            else
                std::cout << RED << "You lose the round." << RESET << "\n";
            std::cout << "Score: " << myScore << " - " << opponentScore << "\n";
            std::cout << CYAN << "============================\n" << RESET;
        } else if (msg.rfind("GAMEOVER ", 0) == 0) {
            int winner = std::stoi(msg.substr(9, 1));
            size_t p1 = msg.find(' ', 11);
            int score1 = std::stoi(msg.substr(11, p1 - 11));
            int score2 = std::stoi(msg.substr(p1 + 1));
            int myScore = (playerNumber == 1) ? score1 : score2;
            int opponentScore = (playerNumber == 1) ? score2 : score1;
            std::cout << "\n" << CYAN << "========== GAME OVER =========\n" << RESET;
            if (winner == playerNumber)
                std::cout << GREEN << "YOU ARE THE CHAMPION!" << RESET << "\n";
            else
                std::cout << YELLOW << "Opponent wins the match." << RESET << "\n";
            std::cout << "Final score: " << myScore << " - " << opponentScore << " (you - opponent)\n";
            break;
        }
    }

    std::cout << "\nConnection closed. Press Enter to exit...";
    std::string dummy;
    std::getline(std::cin, dummy);
    closeSocket(sock);
    stopSockets();
    return 0;
}
