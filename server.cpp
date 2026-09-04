// Simple 2-player Rock Paper Scissors server.
// Build on Windows: g++ server.cpp -std=c++17 -O2 -o server.exe -lws2_32
// Build on Linux/macOS: g++ server.cpp -std=c++17 -O2 -pthread -o server

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

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

std::string cleanName(std::string name) {
    name.erase(std::remove_if(name.begin(), name.end(), [](unsigned char c) {
        return c == '\n' || c == '\r';
    }), name.end());
    if (name.empty()) name = "Player";
    if (name.size() > 20) name.resize(20);
    return name;
}

char cleanMove(const std::string& s) {
    if (s.empty()) return '?';
    char c = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    return (c == 'R' || c == 'P' || c == 'S') ? c : '?';
}

int winner(char a, char b) {
    if (a == b) return 0;
    if ((a == 'R' && b == 'S') || (a == 'P' && b == 'R') || (a == 'S' && b == 'P')) return 1;
    return 2;
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

    Socket listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == BAD_SOCKET) {
        std::cerr << "Could not create server socket.\n";
        stopSockets();
        return 1;
    }

    int reuse = 1;
#ifdef _WIN32
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#else
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        listen(listenSocket, 2) != 0) {
        std::cerr << "Could not bind/listen on port " << PORT << ".\n";
        closeSocket(listenSocket);
        stopSockets();
        return 1;
    }

    std::cout << CYAN;
    std::cout << "======================================\n";
    std::cout << "      ROCK PAPER SCISSORS SERVER      \n";
    std::cout << "======================================\n" << RESET;
    std::cout << "Waiting for 2 players on port " << PORT << "...\n\n";

    std::array<Socket, 2> players{BAD_SOCKET, BAD_SOCKET};
    std::array<std::string, 2> names{"Player 1", "Player 2"};

    for (int i = 0; i < 2; ++i) {
        sockaddr_in clientAddr{};
#ifdef _WIN32
        int clientLen = sizeof(clientAddr);
#else
        socklen_t clientLen = sizeof(clientAddr);
#endif
        players[i] = accept(listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (players[i] == BAD_SOCKET) {
            std::cerr << "Accept failed.\n";
            for (Socket s : players) if (s != BAD_SOCKET) closeSocket(s);
            closeSocket(listenSocket);
            stopSockets();
            return 1;
        }

        sendLine(players[i], "PLAYER " + std::to_string(i + 1));
        sendLine(players[i], "NAME");
        std::string name;
        if (!recvLine(players[i], name)) {
            std::cerr << "Player disconnected during login.\n";
            for (Socket s : players) if (s != BAD_SOCKET) closeSocket(s);
            closeSocket(listenSocket);
            stopSockets();
            return 1;
        }
        names[i] = cleanName(name);
        std::cout << GREEN << names[i] << " connected.\n" << RESET;
    }

    closeSocket(listenSocket);
    sendLine(players[0], "START " + names[0] + " " + names[1]);
    sendLine(players[1], "START " + names[0] + " " + names[1]);

    int score[2] = {0, 0};
    int round = 1;

    while (score[0] < 3 && score[1] < 3) {
        std::cout << YELLOW << "\n--- Round " << round << " ---\n" << RESET;
        std::cout << names[0] << " vs " << names[1] << "\n";

        for (int i = 0; i < 2; ++i) {
            if (!sendLine(players[i], "MOVE")) {
                std::cerr << RED << "A player disconnected.\n" << RESET;
                for (Socket s : players) closeSocket(s);
                stopSockets();
                return 1;
            }
        }

        std::array<char, 2> moves{'?', '?'};
        std::array<std::thread, 2> threads;
        for (int i = 0; i < 2; ++i) {
            threads[i] = std::thread([&, i]() {
                std::string input;
                if (recvLine(players[i], input)) moves[i] = cleanMove(input);
            });
        }
        for (auto& t : threads) t.join();

        if (moves[0] == '?' || moves[1] == '?') {
            std::cerr << RED << "Invalid move or player disconnected.\n" << RESET;
            for (Socket s : players) closeSocket(s);
            stopSockets();
            return 1;
        }

        int w = winner(moves[0], moves[1]);
        if (w == 1) ++score[0];
        if (w == 2) ++score[1];

        std::string result = "RESULT " + std::string(1, moves[0]) + " " +
                             std::string(1, moves[1]) + " " + std::to_string(w) + " " +
                             std::to_string(score[0]) + " " + std::to_string(score[1]);
        for (int i = 0; i < 2; ++i) {
            if (!sendLine(players[i], result)) {
                std::cerr << RED << "A player disconnected.\n" << RESET;
                for (Socket s : players) closeSocket(s);
                stopSockets();
                return 1;
            }
        }

        if (w == 0)
            std::cout << "Draw: " << moveName(moves[0]) << " vs " << moveName(moves[1]) << "\n";
        else
            std::cout << GREEN << "Winner: " << names[w - 1] << RESET << " ("
                      << moveName(moves[w - 1]) << ")\n";
        std::cout << "Score: " << names[0] << " " << score[0] << " - "
                  << score[1] << " " << names[1] << "\n";

        ++round;
    }

    int matchWinner = (score[0] >= 3) ? 1 : 2;
    std::string gameOver = "GAMEOVER " + std::to_string(matchWinner) + " " +
                           std::to_string(score[0]) + " " + std::to_string(score[1]);
    for (Socket s : players) sendLine(s, gameOver);

    std::cout << GREEN << "\nMATCH WINNER: " << names[matchWinner - 1]
              << "\nFinal score: " << score[0] << " - " << score[1] << "\n" << RESET;

    for (Socket s : players) closeSocket(s);
    stopSockets();
    return 0;
}
