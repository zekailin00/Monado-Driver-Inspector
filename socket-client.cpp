#include "offload_protocol.h"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <pthread.h>
#include <stdbool.h>
#include <arpa/inet.h>

#define PORT 8080

#define SOCKET_CHECK(status) if (status < 0)    \
    {                                           \
        perror("Failed: " #status "\n");        \
        exit(EXIT_FAILURE);                     \
    }

#define STATUS_CHECK(status, msg) if (status)   \
    {                                           \
        perror("ERROR: " #msg "\n");            \
        exit(EXIT_FAILURE);                     \
    }

#define LOG(msg) do {           \
        printf("%s\n", msg);    \
    } while(0)
// #define LOG(msg)

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

#include "inspector.h"

int client_fd;
int opt = 1;

float posePayload[7];
unsigned char* imagePayload;

void socket_thread(void* callbackData, unsigned char** image, int* out_width, int* out_height)
{
    int* extent = (int*)callbackData;
    *image = imagePayload;
    *out_width = extent[0];
    *out_height = extent[1];

    ImGui::Begin("Render Config");
    ImGui::Text("Camera:");
    ImGui::SliderFloat3("Camera Position", &posePayload[4], -20.0f, 20.0f);
    ImGui::SliderFloat4("Camera Rotation", posePayload, -10.0f, 10.0f);
    ImGui::End();

    // Handle RX
    message_packet_t packet;
    while (recv(client_fd, &packet.header, sizeof(header_t), MSG_PEEK | MSG_DONTWAIT) > 0)
    {
        LOG("DEBUG: new data is arriving");

        ssize_t received = recv(client_fd, &packet.header, sizeof(header_t), 0);
        STATUS_CHECK(received != sizeof(header_t), "Error receiving header");

        if (packet.header.payload_size != 0)
        {
            int payload_size = packet.header.payload_size;
            packet.payload = (char *) malloc(payload_size);
            STATUS_CHECK(packet.payload == NULL, "Error allocating memory for message data");
        
            size_t bytes_received = 0;
            while (bytes_received < payload_size)
            {
                size_t chunk_size = MIN(1024ul, payload_size - bytes_received);
                char* data_ptr = packet.payload + bytes_received;
                ssize_t chunk_bytes_received = recv(client_fd, data_ptr, chunk_size, 0);

                STATUS_CHECK(chunk_bytes_received == -1, "Error receiving message data");
                bytes_received += chunk_bytes_received;
            }
        }

        if (packet.header.command == CS_REQ_POSE)
        {
            packet.header.command = CS_RSP_POSE;
            packet.header.payload_size = 28;
            packet.payload = (char*)posePayload;

            size_t size_sent = send(client_fd, &packet, sizeof(header_t), 0);
            STATUS_CHECK(size_sent != sizeof(header_t), "DEBUG: failed to send header");

            if (packet.header.payload_size != 0)
            {
                size_t index = 0;
                while (index < packet.header.payload_size)
                {
                    size_t chunk_size = MIN(1024ul, packet.header.payload_size - index);
                    const char* data_ptr = packet.payload + index;
                    ssize_t bytes_sent = send(client_fd, data_ptr, chunk_size, 0);

                    LOG("DEBUG: sent pose");
                    STATUS_CHECK(bytes_sent == -1, "DEBUG: failed to send everything");
                    index += bytes_sent;
                }
            }

        }
        else if (packet.header.command == CS_REQ_IMG)
        {
            memcpy(imagePayload, packet.payload, packet.header.payload_size);
        }
        else if (packet.header.command == CS_GRANT_TOKEN)
        {
            printf("Token granted\n");
        }
        else if (packet.header.command == CS_DEFINE_STEP)
        {
            printf("Step Defined: %d\n", *((int*)packet.payload));
        }
    }
}


int main()
{
    SOCKET_CHECK((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0);

    struct sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
 
    // Convert IPv4 and IPv6 addresses from text to binary form
    SOCKET_CHECK(inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) <= 0);
    SOCKET_CHECK((connect(client_fd, (struct sockaddr*)&address, sizeof(address))) < 0);

    posePayload[0] = 0;
    posePayload[1] = 0;
    posePayload[2] = 0;
    posePayload[3] = 1;

    int extent[2] = {1280, 720};
    int imageSize = extent[0] * extent[1] * 4;
    imagePayload = (unsigned char*)malloc(imageSize);
    for (int i = 0; i < imageSize; i++)
    {
        imagePayload[i] = 0;
    }

    renderLoop(socket_thread, (void*) extent);
}
