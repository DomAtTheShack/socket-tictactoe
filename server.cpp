/**
 * Server Application
 */

#include "JsonSocket.h"
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm> // for std::count

using json = nlohmann::json;
const int PORT = 8080;

// Logic Prototypes
int checkWinner(const std::array<int, 9>& board);
bool isBoardFull(const std::array<int, 9>& board);

// Helper to send JSON
bool sendJSON(int sock, const json& data) {
    std::string jsonPayload = JsonConverter::toString(data);
    if (send(sock, jsonPayload.c_str(), jsonPayload.length(), 0) < 0) return false;
    return true;
}

// Helper to receive JSON
json receivePlayerJSON(int sock) {
    char localBuffer[4096] = {0};
    int bytesRead = read(sock, localBuffer, 4096);

    if (bytesRead <= 0) {
        return json{}; // Return empty JSON on disconnect
    }
    return JsonConverter::fromString(localBuffer);
}

// Ensure these prototypes are visible above the function or in a header
// int checkWinner(const std::array<int, 9>& board);
// bool isBoardFull(const std::array<int, 9>& board);

void handleGameSession(int player1_socket, int player2_socket) {
    std::cout << "[Game] Session Started: P1(" << player1_socket << ") vs P2(" << player2_socket << ")\n";

    // WRAP IN TRY/CATCH: This prevents one game crashing the whole server
    try {
        // 1. Send Welcome Messages
        json welcome1 = {{"type", "welcome"}, {"player_id", 1}, {"message", "Connected! You are Player 1"}};
        sendJSON(player1_socket, welcome1);

        json welcome2 = {{"type", "welcome"}, {"player_id", 2}, {"message", "Connected! You are Player 2"}};
        sendJSON(player2_socket, welcome2);

        bool sessionRunning = true;

        while (sessionRunning) {
            // ================= PLAYER 1 TURN =================
            // Server waits here for Player 1 to send data
            json p1_move = receivePlayerJSON(player1_socket);

            // Handle Disconnect (Empty JSON)
            if (p1_move.empty()) {
                std::cout << "[Game] Player 1 disconnected.\n";
                json quitMsg = {{"gameOver", true}, {"winner", 2}, {"message", "Opponent disconnected"}};
                sendJSON(player2_socket, quitMsg);
                break;
            }

            // Parse Board & Check Logic
            std::array<int, 9> board = p1_move["table"].get<std::array<int, 9>>();
            int winner = checkWinner(board);

            // Check Win or Draw condition
            if (winner != 0 || isBoardFull(board)) {
                p1_move["gameOver"] = true;
                p1_move["winner"] = winner;

                // IMPORTANT: Send the final result to BOTH players immediately
                sendJSON(player2_socket, p1_move); // Tell Loser/Draw
                sendJSON(player1_socket, p1_move); // Tell Winner/Draw

                std::cout << "[Game] Game Over. Winner: " << winner << "\n";
                break; // Exit loop to close sockets
            }

            // Game continues: Forward P1's move to P2
            if (!sendJSON(player2_socket, p1_move)) break;


            // ================= PLAYER 2 TURN =================
            // Server waits here for Player 2
            json p2_move = receivePlayerJSON(player2_socket);

            if (p2_move.empty()) {
                std::cout << "[Game] Player 2 disconnected.\n";
                json quitMsg = {{"gameOver", true}, {"winner", 1}, {"message", "Opponent disconnected"}};
                sendJSON(player1_socket, quitMsg);
                break;
            }

            board = p2_move["table"].get<std::array<int, 9>>();
            winner = checkWinner(board);

            if (winner != 0 || isBoardFull(board)) {
                p2_move["gameOver"] = true;
                p2_move["winner"] = winner;

                // IMPORTANT: Send result to BOTH players
                sendJSON(player1_socket, p2_move);
                sendJSON(player2_socket, p2_move);

                std::cout << "[Game] Game Over. Winner: " << winner << "\n";
                break; // Exit loop to close sockets
            }

            // Game continues: Forward P2's move to P1
            if (!sendJSON(player1_socket, p2_move)) break;
        }

    } catch (const std::exception& e) {
        // This catches JSON parsing errors or other standard C++ errors
        std::cerr << "[Game Error] Exception in game thread (Sockets "
                  << player1_socket << "/" << player2_socket << "): " << e.what() << "\n";
    } catch (...) {
        std::cerr << "[Game Error] Unknown critical error in game thread.\n";
    }

    // CLEANUP: Close sockets so the OS reclaims ports
    std::cout << "[Game] Session Ended. Closing sockets.\n";
    close(player1_socket);
    close(player2_socket);
}

int checkWinner(const std::array<int, 9>& board) {
    // Rows
    if (board[0] == board[1] && board[1] == board[2] && board[0] != 0) return board[0];
    if (board[3] == board[4] && board[4] == board[5] && board[3] != 0) return board[3];
    if (board[6] == board[7] && board[7] == board[8] && board[6] != 0) return board[6];
    // Cols
    if (board[0] == board[3] && board[3] == board[6] && board[0] != 0) return board[0];
    if (board[1] == board[4] && board[4] == board[7] && board[1] != 0) return board[1];
    if (board[2] == board[5] && board[5] == board[8] && board[2] != 0) return board[2];
    // Diagonals
    if (board[0] == board[4] && board[4] == board[8] && board[0] != 0) return board[0];
    if (board[2] == board[4] && board[4] == board[6] && board[2] != 0) return board[2];

    return 0;
}

bool isBoardFull(const std::array<int, 9>& board) {
    // If count of 0s is 0, board is full
    return std::count(board.begin(), board.end(), 0) == 0;
}

int main() {
    int server_fd;
    sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "[Server] Running on port " << PORT << "...\n";
    std::cout << "[Server] Waiting for players...\n"; // Visual confirmation

    std::vector<int> waitingRoom;

    while (true) {
        int new_socket;
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }

        std::cout << "[Server] New connection: " << new_socket << "\n";
        waitingRoom.push_back(new_socket);

        // Check if we have enough players
        if (waitingRoom.size() >= 2) {
            int p1 = waitingRoom[0];
            int p2 = waitingRoom[1];

            // Remove them from the waiting room
            waitingRoom.erase(waitingRoom.begin());
            waitingRoom.erase(waitingRoom.begin());

            std::cout << "[Server] Match found! Starting game thread...\n";

            // Launch the game in the background
            std::thread gameThread(handleGameSession, p1, p2);
            gameThread.detach();

            // The loop immediately continues here to accept NEW players
            std::cout << "[Server] Thread launched. Waiting for NEW players...\n";
        } else {
            std::cout << "[Server] Player added to lobby. Waiting for opponent (1/2)...\n";
        }
    }
    return 0;
}