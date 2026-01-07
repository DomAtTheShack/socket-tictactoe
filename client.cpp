/**
 * Client Application
 */

#include "JsonSocket.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

const int PORT = 8080;
char BUFFER[4096] = {0};

using json = nlohmann::json;

// Forward declarations
bool sendData(int sock, const json& data);
json receiveData(int sock);
bool connectToServer(int& sock);
void drawBoard(int board[]);
json makeTurn(json board, int myId);

bool connectToServer(int& sock, std::string IP) {
    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, IP.data(), &serv_addr.sin_addr) <= 0) {
        std::cout << "\nInvalid address/ Address not supported \n";
        return false;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cout << "\nConnection Failed \n";
        return false;
    }
    return true;
}

bool sendData(int sock, const json& data) {
    std::string jsonPayload = JsonConverter::toString(data);
    if (send(sock, jsonPayload.c_str(), jsonPayload.length(), 0) < 0) {
        perror("Send failed");
        return false;
    }
    return true;
}

json receiveData(int sock) {
    memset(BUFFER, 0, 4096);
    int valread = read(sock, BUFFER, 4096);
    if (valread <= 0) {
        return json{}; // Return empty on error/disconnect
    }
    return JsonConverter::fromString(BUFFER);
}

void drawBoard(int board[]) {
    // QOL: Clear screen terminal
    std::cout << "\033[2J\033[1;1H";

    int count = 0;
    std::string converted[] = {" ", "X", "O"};

    std::cout << "\n";
    std::cout << "  "<< converted[board[count++]] <<" | "<< converted[board[count++]] <<" | "<< converted[board[count++]] <<" \n";
    std::cout << "-------------\n";
    std::cout << "  "<< converted[board[count++]] <<" | "<< converted[board[count++]] <<" | "<< converted[board[count++]] <<" \n";
    std::cout << "-------------\n";
    std::cout << "  "<< converted[board[count++]] <<" | "<< converted[board[count++]] <<" | "<< converted[board[count++]] <<" \n";
    std::cout << "\n";
}

json makeTurn(json board, int myId) {
    std::array<int, 9> table = board["table"].get<std::array<int, 9>>();
    bool validMove = false;

    do {
        int row, col;
        std::cout << "Your Turn (Player " << myId << ")! Enter row and column (ex: 0 2): ";

        // Input validation
        if (!(std::cin >> row >> col)) {
            std::cin.clear();
            std::cin.ignore(1000, '\n');
            std::cout << "Invalid input. Please enter numbers.\n";
            continue;
        }

        int index = row * 3 + col;

        if (row >= 0 && row < 3 && col >= 0 && col < 3 && table[index] == 0) {
            table[index] = myId;
            validMove = true;
        } else {
            std::cout << "Invalid move. Cell taken or out of bounds.\n";
        }

    } while (!validMove);

    board["table"] = table;
    board["playerNum"] = myId;
    return board;
}

int main() {
    int sock = 0;
    std::string IP = "127.0.0.1"; // Change to Server IP if needed

    std::cout << "Enter Server IP: ";
    std::cin >> IP;
    std::cout << "\n";

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;
    if (!connectToServer(sock, IP)) return -1;

    json gameJSON;
    gameJSON["playerNum"] = 0;
    gameJSON["table"] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    gameJSON["gameOver"] = false;
    gameJSON["winner"] = 0;
    gameJSON["type"] = "play";

    std::cout << "Waiting for other player...\n";

    json server_json = receiveData(sock);
    if (server_json.empty() || server_json["type"] != "welcome") {
        std::cerr << "Did not receive welcome from server. Exiting.\n";
        return -1;
    }

    std::cout << ">> " << server_json["message"] << "\n";
    const int myPlayerId = server_json["player_id"];

    bool gameRunning = true;

    while (gameRunning) {

        if (myPlayerId == 1) {
            // --- P1 TURN ---
            drawBoard(gameJSON["table"].get<std::array<int, 9>>().data());

            // Move & Send
            gameJSON = makeTurn(gameJSON, myPlayerId);
            sendData(sock, gameJSON);

            // Wait for P2 (or server saying P1 won)
            std::cout << "Waiting for Player 2...\n";
            json response = receiveData(sock);

            if (response.empty()) break;
            gameJSON = response;

            if (gameJSON["gameOver"]) {
                drawBoard(gameJSON["table"].get<std::array<int, 9>>().data());
                break;
            }

        } else {
            // --- P2 TURN ---
            std::cout << "Waiting for Player 1...\n";
            json response = receiveData(sock);

            if (response.empty()) break;
            gameJSON = response;

            drawBoard(gameJSON["table"].get<std::array<int, 9>>().data());

            if (gameJSON["gameOver"]) break;

            // Move & Send
            gameJSON = makeTurn(gameJSON, myPlayerId);
            sendData(sock, gameJSON);

            // Check if P2 won immediately after sending (Wait for server ack?
            // Actually server sends ack to P2 only if P2 wins.
            // To keep sync, P2 should wait for server confirmation or loop?
            // In this simple logic, P2 loops back to "Wait for P1".
            // If P2 won, the loop breaks on the *next* receive or we need a peek.

            // *Fix for P2 winning:*
            // Since Server sends "Win" packet to P1 and P2 immediately:
            // P2 sends move -> Server checks Win -> Server sends "Game Over" to P2.
            // So P2 must do a quick read or handle the 'break' differently.

            // Simplified: P2 sends, then waits for P1's NEXT move OR the "Game Over" ack.
            std::cout << "Sending move and waiting...\n";

            // To support P2 receiving the "You Won" message, we actually need to read once more
            // OR rely on the server sending the "Game Over" as the start of the next loop.
        }
    }

    int winner = gameJSON["winner"];
    if (winner == 0) std::cout << "Game Over: It's a Draw!\n";
    else if (winner == myPlayerId) std::cout << "Game Over: YOU WIN!\n";
    else std::cout << "Game Over: You Lost.\n";

    close(sock);
    return 0;
}