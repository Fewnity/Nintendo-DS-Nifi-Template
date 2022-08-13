#ifndef MAIN_H_ /* Include guard */
#define MAIN_H_

#define EMPTY -1

// Nifi settings
#define MAX_CLIENT 6 // Max number of clients
//#define WIFI_RATE 60 // Read/Write rate per second
#define WIFI_RATE 240        // Read/Write rate per second
#define WIFI_TIMEOUT 10      // Number of wifi rate before the message is considered lost
#define MAX_TIMEOUT_RETRY 10 // Number of try before client is considered as disconnected
#define CLIENT_TIMEOUT 60    // Number of frame before local client is considered as disconnected

// Network packet settings
// request example : {TYPE;NAME;PARAM1;PARAM2}
#define REQUEST_TYPE_INDEX 0
#define REQUEST_NAME_INDEX 1
#define MAX_REQUEST_LENGTH 256
#define MAX_REQUEST_PARAM_LENGTH 64
#define MAX_REQUEST_PARAM_COUNT 10

// Text colors
#define RED "\x1B[31m"
#define GREEN "\x1B[32m"
#define YELLOW "\x1B[33m"
#define BLUE "\x1B[34m"
#define MAGENTA "\x1B[35m"
#define CYAN "\x1B[36m"
#define WHITE "\x1B[37m"

enum RequestType
{
    POSITION = 0
};

typedef struct
{
    int id;                // Id of the client
    char macAddress[13];   // Mac address of the client
    char sendBuffer[1024]; // Wifi write buffer of the client
    int lastMessageId;     // Last message id received by the client
    // bool skipData;         // If true, the client will skip data of the current wifi packet
    int positionX; // X position of the client
    int positionY; // Y position of the client
} Client;

//////All functions
void treatData();
void sendPacketByte(u8 command, u8 data);
void createRoom();
void scanForRoom();
void addClient(int id, bool addHost);
void SendDataTo(Client *client);
void AddDataTo(Client *client, const char *data);
void removeClient(Client *client);
void resetClientValues(Client *client);
void managerServer();
void communicateWithNextClient();
void nifiInit();
Client *getClientById(int consoleId);
void shareRequest(Client *clientSender, enum RequestType requestType);
void createRequest(Client *clientSender, Client *clientToUpdate, enum RequestType requestType);
void resetNifiValues();

#endif // MAIN_H_