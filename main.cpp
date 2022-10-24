#include "tcpserver.h"

int main()
{
        // create server - it starts automatically in constructor
        TCPServer server;

        // server.stop(); // we could stop the server this way

        // wait for server thread to end
        server.join();

        return 0;
}
