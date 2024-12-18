#include <iostream>
#include <string>
#include <algorithm>
#include <winsock2.h>
#include <ws2tcpip.h> // Required for inet_pton

#pragma comment(lib, "ws2_32.lib")

#define PORT 4040
#define SERVER_IP "127.0.0.1" // Change this to your server's IP if needed

int main() {
    WSADATA wsa;
    SOCKET clientSocket;
    sockaddr_in serverAddr;
    char buffer[256];

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "Winsock initialization failed. Error Code: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // Create socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed. Error Code: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Set server address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);

    // Convert IP address to binary form
    if (inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid IP address: " << SERVER_IP << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    // Connect to the server
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connecting to server failed. Error Code: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to the server!" << std::endl;

    while (true) {
        // Get user input
        std::string choice;
        std::cout << "Enter your choice (ROCK, PAPER, SCISSORS), 'score' to view your score, or 'end' to quit: ";
        std::cin >> choice;

        // Convert input to uppercase for consistency
        std::transform(choice.begin(), choice.end(), choice.begin(), ::toupper);

        // Handle the "END" command
        if (choice == "END") {
            std::cout << "Ending the game..." << std::endl;
            break;
        }

        // Handle the "SCORE" command
        if (choice == "SCORE") {
            if (send(clientSocket, choice.c_str(), choice.length(), 0) == SOCKET_ERROR) {
                std::cerr << "Sending data failed. Error Code: " << WSAGetLastError() << std::endl;
                break;
            }
            std::cout << "Requested score from the server." << std::endl;

            // Receive the server's response
            memset(buffer, 0, sizeof(buffer));
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            if (bytesReceived <= 0) {
                std::cerr << "Receiving data failed. Error Code: " << WSAGetLastError() << std::endl;
                break;
            }

            buffer[bytesReceived] = '\0'; // Null-terminate the received data
            std::cout << "Server response: " << buffer << std::endl;
            continue;
        }

        // Validate input for the game (no change needed as the input is now case-insensitive)
        if (choice != "ROCK" && choice != "PAPER" && choice != "SCISSORS") {
            std::cerr << "Invalid choice! Please choose ROCK, PAPER, SCISSORS, or type SCORE/END." << std::endl;
            continue;
        }

        // Send the choice to the server
        if (send(clientSocket, choice.c_str(), choice.length(), 0) == SOCKET_ERROR) {
            std::cerr << "Sending data failed. Error Code: " << WSAGetLastError() << std::endl;
            break;
        }

        std::cout << "Sent: " << choice << std::endl;

        // Receive the server's response
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            std::cerr << "Receiving data failed. Error Code: " << WSAGetLastError() << std::endl;
            break;
        }

        buffer[bytesReceived] = '\0'; // Null-terminate the received data
        std::cout << "Server response: " << buffer << std::endl;
    }

    // Close the connection
    closesocket(clientSocket);
    WSACleanup();
    return 0;
}
