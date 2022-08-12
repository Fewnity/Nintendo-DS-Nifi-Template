#include <nds.h>
#include <dswifi9.h>

#include <stdio.h>
#include "main.h"
#include <gl2d.h>

////// Wifi variables :
int wifiChannel = 10;
//// Wifi buffers:
// Read part
char WIFI_Buffer[1024] = "";			// Wifi buffer is the incoming data from the wifi (DO NOT USE LIKE THIS, please use the "wirelessData" define)
#define wirelessData (WIFI_Buffer + 32) // Define to use the WIFI_Buffer array to avoid wrong values
char Values[1024] = "";					// Store all the values received from the wifi and wainting for treatment
char TempValues[1024] = "";				// Store values that can't be treated yet
int WIFI_ReceivedDataLength = 0;		// Size of data received from the wifi
// Write part
char tempSendBuffer[1024] = ""; // Temp buffer to use when the host need to resend the same data if the wifi is not working correctly

////// Room variables :
Client clients[MAX_CLIENT];
Client *localClient = &clients[0];
int hostIndex = EMPTY; // Index of the host in the clients array
bool isHost = false;   // Is the Nintendo DS hosting a room?
// For host
int speakTo = 1;		   // Index of the speak to speak with
int idCount = 0;		   // Id of the next client to add
int lastCommunication = 0; // Countdown to check the wifi timeout
char tempMacAddress[13];   // Mac address of the client that is trying to join the room
int timeoutCount = 0;	   // Store how many times the wifi has failed to communicate with the client
// For non-host
int joinRoomTimer = WIFI_TIMEOUT; // Timer to limit the time to join a room
bool tryJoinRoom = false;		  // Is the client trying to join a room

////TO DO
// Automatic channel selection (Check how many random packet are received on each channel and select the best one)
// Reset lastMessageId to 0 if the number is too high to not hit the int limit (maybe useless but great to have)

int main(void)
{
	// Enable 3D engine
	videoSetMode(MODE_5_3D);

	consoleDemoInit();
	//  Initialize GL
	glScreen2D();
	// Swap screens
	lcdMainOnBottom();

	// Init WIFI (nifi mode)
	nifiInit();

	while (1)
	{
		scanKeys();
		// Get stylus location
		touchPosition touchXY;
		touchRead(&touchXY);
		// Read keys
		int keys = keysHeld();
		int keysdown = keysDown();

		// Check if stylus is down
		if (keys & KEY_TOUCH)
		{
			// Store stylus location
			localClient->positionX = touchXY.px;
			localClient->positionY = touchXY.py;
			// Ask for sending data to all clients
			for (int i = 1; i < MAX_CLIENT; i++)
			{
				clients[i].sendPosition = true;
			}
		}

		// Check for action : join room, create room
		if (keysdown & KEY_UP)
		{
			createRoom();
		}

		if (keysdown & KEY_DOWN)
		{
			tryJoinRoom = true;
		}
		// If the client is trying to join a room, scan for a room
		if (tryJoinRoom)
		{
			joinRoomTimer--;
			if (joinRoomTimer == 0) // Resend the request each time the timer is ended
			{
				scanForRoom();
				// Reset the timer
				joinRoomTimer = WIFI_TIMEOUT;
			}
		}

		if (localClient->id != EMPTY && !isHost)
		{
			lastCommunication++;
			if (lastCommunication == CLIENT_TIMEOUT)
			{
				for (int i = 0; i < MAX_CLIENT; i++)
				{
					removeClient(&clients[i]);
				}
			}
		}

		// Draw client stylus position
		glBegin2D();

		for (int i = 0; i < MAX_CLIENT; i++)
		{
			if (clients[i].id != EMPTY)
			{
				int color = RGB15(31, 31, 31);
				if (i == 0)
					// Change the color of the local player
					color = RGB15(0, 10, 31);

				// draw a box at the client position
				glBoxFilled(clients[i].positionX - 3, clients[i].positionY - 3,
							clients[i].positionX + 3, clients[i].positionY + 3,
							color);
			}
		}

		glEnd2D();
		glFlush(0);
		swiWaitForVBlank();
	}
}

/**
 * @brief Wifi handler call when data is received
 *
 * @param packetID
 * @param readlength
 */
void WirelessHandler(int packetID, int readlength)
{
	Wifi_RxRawReadPacket(packetID, readlength, (unsigned short *)WIFI_Buffer);

	// Get the correct data length
	WIFI_ReceivedDataLength = readlength - 32;

	// Treatment of the data
	treatData();
}

/**
 * @brief Send data to clients
 *
 * @param buffer
 * @param length
 */
void SendWirelessData(unsigned short *buffer, int length)
{
	if (Wifi_RawTxFrame(length, 0x0014, buffer) != 0)
	{
		iprintf("Error calling RawTxFrame\n");
	}
}

/**
 * @brief Init the Nifi system
 *
 */
void nifiInit()
{
	// Reset clients data
	for (int i = 0; i < MAX_CLIENT; i++)
	{
		resetClientValues(&clients[i]);
	}
	// Reset buffers
	WIFI_Buffer[0] = '\0';
	Values[0] = '\0';
	TempValues[0] = '\0';
	tempSendBuffer[0] = '\0';
	// Reset variables
	lastCommunication = 0;
	hostIndex = EMPTY;
	WIFI_ReceivedDataLength = 0;

	iprintf("\nInit NiFi...\n");

	// Changes how packets are handled
	Wifi_SetRawPacketMode(PACKET_MODE_NIFI);

	// Init Wifi without automatic settings
	Wifi_InitDefault(false);

	// Enable wireless
	Wifi_EnableWifi();

	// Configure custom packet handler for when
	Wifi_RawSetPacketHandler(WirelessHandler);

	// Force specific channel for communication
	Wifi_SetChannel(wifiChannel);

	// Get MAC address of the Nintendo DS
	u8 macAddressUnsigned[6];
	Wifi_GetData(WIFIGETDATA_MACADDRESS, 6, macAddressUnsigned);
	// Convert unsigned values to hexa values
	sprintf(localClient->macAddress, "%02X%02X%02X%02X%02X%02X", macAddressUnsigned[0], macAddressUnsigned[1], macAddressUnsigned[2], macAddressUnsigned[3], macAddressUnsigned[4], macAddressUnsigned[5]);

	printf("NiFi initiated.\n\n");
	// printf("%s\n\n", localClient->macAddress);
	printf("%sUp to create a room.\n%s", YELLOW, WHITE);
	printf("%sDown to join the room.\n%s", CYAN, WHITE);
}

/*
 * Treat data from the wifi module
 */
void treatData()
{
	// Get data lenght
	int recvd_len = strlen(wirelessData);

	// Check if the packet is valid
	if (WIFI_ReceivedDataLength == recvd_len + 1)
	{
		// Add the wifi buffer to the data buffer
		strcat(Values, wirelessData);

		int StartPosition, EndPosition;
		// Get data of the packet
		while ((StartPosition = strstr(Values, "{") - Values + 1) > 0 && (EndPosition = strstr(Values + StartPosition, "}") - Values) > 0)
		{
			char currentPacket[MAX_REQUEST_LENGTH] = "";
			strncpy(currentPacket, Values + StartPosition, EndPosition - StartPosition);

			// Start spliting incoming data
			char *ptr = strtok(currentPacket, ";");
			int SplitCount = 0;
			char params[MAX_REQUEST_PARAM_COUNT][MAX_REQUEST_PARAM_LENGTH] = {0};

			while (ptr != NULL)
			{
				strcpy(params[SplitCount], ptr);
				SplitCount++;
				ptr = strtok(NULL, ";");
			}

			if (strcmp(params[REQUEST_TYPE_INDEX], "GAME") == 0 && !localClient->skipData) // Check if the request is about game management (refuse the request if the request was already treated)
			{
				if (strcmp(params[REQUEST_NAME_INDEX], "POSITION") == 0) // A client is sending his position
				{
					int clientId, destinatorId;
					// Get the client destinator id
					sscanf(params[3], "%d", &destinatorId);

					// If the local client is in a party and the destinator id is the same as the local client id
					if (destinatorId == localClient->id && localClient->id != EMPTY)
					{
						// Get the if of the client that sent the request
						sscanf(params[2], "%d", &clientId);

						Client *client = getClientById(clientId);
						if (client != NULL)
						{
							// Get the value
							sscanf(params[4], "%d", &client->positionX);
							sscanf(params[5], "%d", &client->positionY);
							printf("%s%d POSITION : %d %d%s\n", GREEN, clientId, client->positionX, client->positionY, WHITE);

							// If the local client is the host
							if (isHost)
							{
								// Send the data to all clients
								for (int clientIndex = 1; clientIndex < MAX_CLIENT; clientIndex++)
								{
									Client *clientToUpdate = &clients[clientIndex];
									// Avoid sending the request to the current treated client
									if (clientId != clientToUpdate->id && clientToUpdate->id != EMPTY)
									{
										char buffer[28];
										sprintf(buffer, "{GAME;POSITION;%d;%d;%d;%d}", clientId, clientToUpdate->id, client->positionX, client->positionY);
										AddDataTo(clientIndex, buffer);
									}
								}
							}
						}
					}
				}
			}
			else if (strcmp(params[REQUEST_TYPE_INDEX], "ROOM") == 0) // Check if the request is about room management
			{
				if (isHost) // These request are for the host client
				{
					if (strcmp(params[REQUEST_NAME_INDEX], "SCAN") == 0) // A client is searching for a room
					{
						printf("%s SEARCH FOR ROOM\n", params[2]);

						// Get the mac address of the client
						sprintf(tempMacAddress, params[2]);
						addClient(EMPTY, false);
					}
					else if (strcmp(params[REQUEST_NAME_INDEX], "CONFIRM_LISTEN") == 0) // If the client has received host's data
					{
						// Get the id of the client that has received the data
						int clientId;
						sscanf(params[2], "%d", &clientId);

						// If the id of the client is the same as the id of current treated client
						if (clientId == clients[speakTo].id)
						{
							communicateWithNextClient();
						}
					}
				}
				else // These request are for non-host clients
				{
					if (strcmp(params[REQUEST_NAME_INDEX], "CONFIRM_JOIN") == 0) // The host said that the local client can join the room
					{
						// Check if the mac address of the local client is the same as the one of the client that wants to join the room
						if (strcmp(params[2], localClient->macAddress) == 0 && localClient->id == EMPTY)
						{
							// Get the host id
							int hostId;
							sscanf(params[3], "%d", &hostId);
							// Set local player id
							sscanf(params[4], "%d", &localClient->id);
							addClient(hostId, true);
							printf("JOINED %d'S ROOM, YOUR ID: %d\n", hostId, localClient->id);
							tryJoinRoom = false;
						}
					}
					else if (strcmp(params[REQUEST_NAME_INDEX], "WANTSPEAK") == 0) // The host asked for communication
					{
						int clientId;
						sscanf(params[2], "%d", &clientId);
						lastCommunication = 0;
						// If the host wants to communicate with the local client

						if (localClient->id == clientId)
						{
							int messageId;
							sscanf(params[3], "%d", &messageId);

							// If the request wasn't read yet
							if (localClient->lastMessageId < messageId)
							{
								// Clear temp buffer
								strcpy(tempSendBuffer, "");
								localClient->skipData = false;
								localClient->lastMessageId = messageId;
							}
							else // If the request was already read
							{
								// Skip the request data
								localClient->skipData = true;
							}

							SendDataTo(hostIndex);
						}
					}
					else if (strcmp(params[REQUEST_NAME_INDEX], "ADDRANGE") == 0) // Add multiples non local players
					{
						int destinatorId;
						sscanf(params[2], "%d", &destinatorId);
						if (destinatorId == localClient->id)
						{
							for (int i = 3; i < SplitCount; i++)
							{
								int FoundId = EMPTY;
								sscanf(params[i], "%d", &FoundId);
								addClient(FoundId, false);
							}
						}
					}
				}
				// For the host and non-host clients

				if (strcmp(params[REQUEST_NAME_INDEX], "QUIT") == 0 && !localClient->skipData) // A client quit the party
				{
					int clientId;
					sscanf(params[2], "%d", &clientId);
					int destinatorId;
					sscanf(params[3], "%d", &destinatorId);

					if (localClient->id == destinatorId)
					{
						Client *client = getClientById(clientId);
						if (client != NULL)
						{
							removeClient(client);
						}

						printf("%d HAS LEFT THE ROOM\n", clientId);
					}
				}
			}

			// Clear "TempValues"
			for (int i = 0; i < sizeof(TempValues); i++)
				TempValues[i] = '\0';

			// Add all characters after current data packet to "TempValues"
			strcat(TempValues, Values + EndPosition + 1);
			// Copy "TempValues" to "Values"
			strcpy(Values, TempValues);
		}
	}
}

/**
 * @brief Get the Client By Id (doens't return the local player)
 *
 * @param clientId
 * @return Client*
 */
Client *getClientById(int clientId)
{
	for (int i = 1; i < MAX_CLIENT; i++)
	{
		if (clients[i].id == clientId)
		{
			return &clients[i];
		}
	}
	return NULL;
}

/**
 * @brief Create a room
 *
 */
void createRoom()
{
	// Is the client is not already a host
	if (!isHost)
	{
		// Create a room
		isHost = true;
		idCount = 0;
		localClient->id = idCount;
		timerStart(0, ClockDivider_1024, TIMER_FREQ_1024(WIFI_RATE), managerServer);
		printf("Room created\n");
	}
}

/**
 * @brief Remove a client
 *
 * @param index Client's index to remove
 */
void removeClient(Client *client)
{
	if (client->id != EMPTY)
	{
		if (isHost)
		{
			for (int i = 1; i < MAX_CLIENT; i++)
			{
				if (&clients[i] != client && clients[i].id != EMPTY)
				{
					char buffer[18];
					sprintf(buffer, "{ROOM;QUIT;%d;%d}", client->id, clients[i].id);
					AddDataTo(i, buffer);
				}
			}
		}
		printf("%sCLIENT REMOVED : %d%s\n", RED, client->id, WHITE);
	}
	resetClientValues(client);
}

/**
 * @brief Reset client values
 *
 * @param client pointer to reset
 */
void resetClientValues(Client *client)
{
	client->id = EMPTY;
	strcpy(client->macAddress, "");
	strcpy(client->sendBuffer, "");
	client->lastMessageId = 0;
	client->skipData = false;
}

/**
 * @brief Add a client
 *
 * @param id Client's id (not used by the room owner, set to -1/EMPTY)
 * @param addHost Are we adding the host?
 */
void addClient(int id, bool addHost)
{
	bool macAlreadyExists = false;
	bool idAlreadyExists = false;
	if (isHost) // If the local client is the host
	{
		// Check if the client to add is already in the room (same mac address), because the client is maybe trying to join the room multiple times if the wifi is not working properly
		for (int i = 1; i < MAX_CLIENT; i++)
		{
			if (strcmp(tempMacAddress, clients[i].macAddress) == 0)
			{
				macAlreadyExists = true;
				break;
			}
		}
	}
	else
	{
		// Check if the client to add is already in the room (same id)
		for (int i = 1; i < MAX_CLIENT; i++)
		{
			if (clients[i].id == id)
			{
				idAlreadyExists = true;
				break;
			}
		}
	}

	// If the client to add is not already in the room
	if (!macAlreadyExists && !idAlreadyExists)
	{
		int addedClientIndex = EMPTY;
		for (int i = 1; i < MAX_CLIENT; i++)
		{
			if (clients[i].id == EMPTY)
			{
				if (isHost)
				{
					// Set client id
					idCount++;
					clients[i].id = idCount;
					// Set client mac address
					sprintf(clients[i].macAddress, tempMacAddress);
					printf("ADDED AT %d, ID : %d\n", i, idCount);
				}
				else
				{
					printf("CLIENT ADDED : %d\n", id);
					// Apply id
					clients[i].id = id;
				}

				// Store the host index
				if (addHost)
					hostIndex = i;
				addedClientIndex = i;
				break;
			}
		}

		// If the client is added by the host
		if (addedClientIndex != EMPTY && isHost)
		{
			// Send the client his id
			char buffer[100];
			sprintf(buffer, "{ROOM;CONFIRM_JOIN;%s;%d;%d}", tempMacAddress, localClient->id, clients[addedClientIndex].id);
			// Send the client all the other clients ids
			sprintf(buffer + strlen(buffer), "{ROOM;ADDRANGE;%d", clients[addedClientIndex].id);
			// Send the client id to all the other clients
			for (int i = 1; i < MAX_CLIENT; i++)
			{
				if (clients[i].id != EMPTY && i != addedClientIndex)
				{
					sprintf(buffer + strlen(buffer), ";%d", clients[i].id);
					char buffer2[22];
					sprintf(buffer2, "{ROOM;ADDRANGE;%d;%d}", clients[i].id, clients[addedClientIndex].id);
					AddDataTo(i, buffer2);
				}
			}
			sprintf(buffer + strlen(buffer), "}");
			AddDataTo(addedClientIndex, buffer);
		}
	}
}

/**
 * @brief Scan for a room
 *
 */
void scanForRoom()
{
	//  Join a room
	isHost = false;

	char buffer[25];
	sprintf(buffer, "{ROOM;SCAN;%s}", localClient->macAddress);
	SendWirelessData((unsigned short *)buffer, strlen(buffer) + 1);
}

/**
 * @brief Select the next client to communicate with
 *
 */
void communicateWithNextClient()
{
	// Reset values
	timeoutCount = 0;
	lastCommunication = 0;

	// Change client to communicate with
	speakTo++;
	if (speakTo == MAX_CLIENT) // Go back to the beginning of the list
		speakTo = 1;

	if (clients[speakTo].id != EMPTY)
	{
		AddDataTo(speakTo, "{}"); // TO REMOVE FIX : Data can't be sent if the buffer is empty
		SendDataTo(speakTo);
	}
	else
	{
		lastCommunication = WIFI_TIMEOUT;
	}
}

/**
 * @brief Manage client's communication order and timeout
 *
 */
void managerServer()
{
	if (isHost)
	{
		lastCommunication++;
		if (lastCommunication == WIFI_TIMEOUT + 1)
		{
			communicateWithNextClient();
		}

		if (lastCommunication == WIFI_TIMEOUT)
		{
			if (speakTo != 0 && clients[speakTo].id != EMPTY)
			{
				if (timeoutCount <= MAX_TIMEOUT_RETRY)
				{
					// Restart
					timeoutCount++;
					lastCommunication = 0;

					SendDataTo(speakTo);
				}
				else
				{
					removeClient(&clients[speakTo]);
					communicateWithNextClient();
				}
			}
		}
	}
}

void AddDataTo(int playerIndex, const char *data)
{
	if (playerIndex != EMPTY)
	{
		if (clients[playerIndex].id != EMPTY)
			sprintf(clients[playerIndex].sendBuffer + strlen(clients[playerIndex].sendBuffer), "%s", data);
	}
	else
	{
		for (int i = 1; i < MAX_CLIENT; i++)
		{
			if (clients[i].id != EMPTY)
				sprintf(clients[i].sendBuffer + strlen(clients[i].sendBuffer), "%s", data);
		}
	}
}

void SendDataTo(int playerIndex)
{
	char finalBuffer[1024] = "";
	if (isHost)
	{
		if (timeoutCount == 0)
		{
			clients[playerIndex].lastMessageId++;
		}
		sprintf(finalBuffer, "{ROOM;WANTSPEAK;%d;%d}", clients[playerIndex].id, clients[playerIndex].lastMessageId);
	}
	else
		sprintf(finalBuffer, "{ROOM;CONFIRM_LISTEN;%d}", localClient->id);

	// If the buffer is not empty, copy the buffer to a new one and clear the buffer
	if (timeoutCount == 0 && (strlen(clients[playerIndex].sendBuffer) != 0 || strlen(tempSendBuffer) == 0))
	{
		strcpy(tempSendBuffer, "");
		sprintf(tempSendBuffer + strlen(tempSendBuffer), "%s", clients[playerIndex].sendBuffer);
		if (clients[playerIndex].sendPosition)
		{
			sprintf(tempSendBuffer + strlen(tempSendBuffer), "{GAME;POSITION;%d;%d;%d;%d}", localClient->id, clients[playerIndex].id, localClient->positionX, localClient->positionY);
			clients[playerIndex].sendPosition = false;
		}
		strcpy(clients[playerIndex].sendBuffer, "");
	}
	if (strlen(tempSendBuffer) != 0)
		sprintf(finalBuffer + strlen(finalBuffer), "%s", tempSendBuffer);

	SendWirelessData((unsigned short *)finalBuffer, strlen(finalBuffer) + 1);
}