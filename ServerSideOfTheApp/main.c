#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <time.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 4040
#define BUFFER_SIZE 256

typedef enum { ROCK, PAPER, SCISSORS } Choice;

// Converts Choice enum to string
const char* getChoiceName(Choice choice) {
    switch (choice) {
    case ROCK: return "ROCK";
    case PAPER: return "PAPER";
    case SCISSORS: return "SCISSORS";
    default: return "";
    }
}

// Determines the result of the game
const char* determineResult(Choice playerChoice, Choice serverChoice) {
    if (playerChoice == serverChoice) return "It's a tie!";
    if ((playerChoice == ROCK && serverChoice == SCISSORS) ||
        (playerChoice == PAPER && serverChoice == ROCK) ||
        (playerChoice == SCISSORS && serverChoice == PAPER)) {
        return "You win!";
    }
    return "Server wins!";
}

// Converts a string to a Choice enum
int stringToChoice(const char* input, Choice* choice) {
    if (strcmp(input, "ROCK") == 0) {
        *choice = ROCK;
        return 1;
    }
    if (strcmp(input, "PAPER") == 0) {
        *choice = PAPER;
        return 1;
    }
    if (strcmp(input, "SCISSORS") == 0) {
        *choice = SCISSORS;
        return 1;
    }
    return 0; // Invalid choice
}

// Structure to store client scores
typedef struct {
    int wins;
    int losses;
    int ties;
} ClientScore;

// Global variables for client numbering and score tracking
CRITICAL_SECTION clientMutex; // Using Windows Critical Section for thread synchronization
int clientCounter = 0;
ClientScore clientScores[100]; // Support for up to 100 clients

// Function to handle a single client connection
DWORD WINAPI handleClient(LPVOID arg) {
    SOCKET clientSocket = *(SOCKET*)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    int clientId;

    EnterCriticalSection(&clientMutex);
    clientId = ++clientCounter;
    ClientScore newScore = { 0, 0, 0 };
    clientScores[clientId] = newScore;
    LeaveCriticalSection(&clientMutex);

    printf("Client %d connected.\n", clientId);

    srand((unsigned)time(NULL) + clientId); // Unique seed for each client

    while (1) {
        // Receive player's choice
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
        if (bytesReceived <= 0) {
            printf("Client %d disconnected.\n", clientId);
            break;
        }
        buffer[bytesReceived] = '\0'; // Null-terminate the received data

        // Handle "SCORE" command
        if (strcmp(buffer, "SCORE") == 0) {
            EnterCriticalSection(&clientMutex);
            ClientScore score = clientScores[clientId];
            LeaveCriticalSection(&clientMutex);

            char scoreMessage[BUFFER_SIZE];
            snprintf(scoreMessage, BUFFER_SIZE, "Your Score: Wins: %d, Losses: %d, Ties: %d",
                score.wins, score.losses, score.ties);
            send(clientSocket, scoreMessage, strlen(scoreMessage), 0);
            continue;
        }

        // Handle "END" command
        if (strcmp(buffer, "END") == 0) {
            printf("Client %d ended the game.\n", clientId);
            break;
        }

        Choice playerChoice;
        if (!stringToChoice(buffer, &playerChoice)) {
            const char* errorMessage = "Invalid choice. Please choose ROCK, PAPER, or SCISSORS.";
            send(clientSocket, errorMessage, strlen(errorMessage), 0);
            continue;
        }

        // Generate server's random choice
        Choice serverChoice = (Choice)(rand() % 3);

        printf("Client %d chose: %s\n", clientId, getChoiceName(playerChoice));
        printf("Server chose: %s\n", getChoiceName(serverChoice));

        // Determine result
        const char* resultMessage = determineResult(playerChoice, serverChoice);

        char result[BUFFER_SIZE];
        snprintf(result, BUFFER_SIZE, "You chose %s, Server chose %s. %s",
            getChoiceName(playerChoice), getChoiceName(serverChoice), resultMessage);

        // Update score
        EnterCriticalSection(&clientMutex);
        if (playerChoice == serverChoice) {
            clientScores[clientId].ties++;
        }
        else if ((playerChoice == ROCK && serverChoice == SCISSORS) ||
            (playerChoice == PAPER && serverChoice == ROCK) ||
            (playerChoice == SCISSORS && serverChoice == PAPER)) {
            clientScores[clientId].wins++;
        }
        else {
            clientScores[clientId].losses++;
        }
        LeaveCriticalSection(&clientMutex);

        // Send result back to client
        send(clientSocket, result, strlen(result), 0);
    }

    // Close client socket
    closesocket(clientSocket);
    printf("Client %d session ended.\n", clientId);
    return 0;
}

int main() {
    WSADATA wsa;
    SOCKET serverSocket;
    struct sockaddr_in serverAddr, clientAddr;
    int addrLen = sizeof(clientAddr);

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "Winsock initialization failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    // Create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        fprintf(stderr, "Socket creation failed. Error Code: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // Set server address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    // Bind socket to the address and port
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        fprintf(stderr, "Binding failed. Error Code: %d\n", WSAGetLastError());
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Listen for incoming connections
    if (listen(serverSocket, 5) == SOCKET_ERROR) {
        fprintf(stderr, "Listening failed. Error Code: %d\n", WSAGetLastError());
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Initialize critical section
    InitializeCriticalSection(&clientMutex);

    printf("Server is listening on port %d...\n", PORT);

    while (1) {
        // Accept a new client connection
        SOCKET clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &addrLen);
        if (clientSocket == INVALID_SOCKET) {
            fprintf(stderr, "Client connection failed. Error Code: %d\n", WSAGetLastError());
            continue;
        }
        SOCKET* clientSocketPtr = (SOCKET*)malloc(sizeof(SOCKET));
        *clientSocketPtr = clientSocket;
        HANDLE clientThread = CreateThread(NULL, 0, handleClient, clientSocketPtr, 0, NULL);
        if (clientThread == NULL) {
            fprintf(stderr, "Thread creation failed. Error Code: %d\n", GetLastError());
            closesocket(clientSocket);
            free(clientSocketPtr);
            continue;
        }

        // Close the thread handle since it's being handled in the background
        CloseHandle(clientThread);
    }

    // Close the server socket and cleanup Winsock
    closesocket(serverSocket);
    WSACleanup();

    // Clean up critical section
    DeleteCriticalSection(&clientMutex);

    return 0;
}
