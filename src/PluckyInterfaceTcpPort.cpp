#include <WiFiServer.h>
#include <WifiClient.h>
#include <Wifi.h>
#include <ArduinoSimpleLogging.h>

#include "PluckyInterfaceTcpPort.hpp"
#include "PluckyInterfaceTcpClient.hpp"
#include "config.hpp"

PluckyInterfaceTcpPort::PluckyInterfaceTcpPort(uint16_t port) : PluckyInterfaceGroup(TCP_MAX_CLIENTS) {
    _tcpPort = port;
    _tcpServer = WiFiServer(port, TCP_MAX_CLIENTS);
    for (uint16_t i=0; i<TCP_MAX_CLIENTS; i++) {
        _interfaces[i] = new PluckyInterfaceTcpClient();
    }
}

PluckyInterfaceTcpPort::~PluckyInterfaceTcpPort() {
    for (uint16_t i=0; i<TCP_MAX_CLIENTS; i++) {
// This should technically get deleted but since this is in effect a singletion class,
// commenting it out as it makes the compliler mildly grumpy (polymorphic destructor)
//        delete (PluckyInterfaceTcpClient *)(_interfaces[i]);
    }
}

void PluckyInterfaceTcpPort::doLoop() {
    if (WiFi.status() != WL_CONNECTED) {
        if (_tcpServer) {
            end();
        }
    } else {
        if (!_tcpServer) {
            begin();
        }
    }

    // Check for new incoming connections
    if (_tcpServer && _tcpServer.hasClient()) { // find free/disconnected spot
        Logger.info.print("New TCP client ");
        for (uint16_t i = 0; i < _numInterfaces; i ++) {
            if (!((PluckyInterfaceTcpClient *)_interfaces[i])->connected()) {
                Logger.info.printf("in slot: %d\n", i);
                ((PluckyInterfaceTcpClient *)_interfaces[i])->setTcpClient(_tcpServer.available());
                break;
            } else {
                Logger.info.print(". ");
            }
            if (i == _numInterfaces - 1) { // no free/disconnected spot so reject it
                WiFiClient TmpserverClient = _tcpServer.available();
                TmpserverClient.stop();
                Logger.info.println("Too many TCP clients; new connection dropped");
            }   
        }
    }     

    for (uint16_t i=0; i<_numInterfaces; i++) {
        _interfaces[i]->doLoop();
    }
}

void PluckyInterfaceTcpPort::begin() {
    _tcpServer.begin(); // start TCP server
    _tcpServer.setNoDelay(true);
    Logger.info.println("TCP server enabled");
}

void PluckyInterfaceTcpPort::end(){
    Logger.info.println("Stopping TCP server");
    _tcpServer.end();
}

bool PluckyInterfaceTcpPort::available() {
    // Note, because readAll() visits and reads ALL interfaces in the group
    // this does not introduce a bias / risk of starvation.
    for (uint16_t i=0; i<_numInterfaces; i++) {
        if (_interfaces[i]->available()) {
            return true;
        }
    }
    return false;
}

bool PluckyInterfaceTcpPort::readAll() {
    bool didRead = false;
    for (uint16_t i=0; i<_numInterfaces; i++) {
        if (_interfaces[i]->available()) {
            didRead = (_interfaces[i]->readAll() || didRead);
        }
    }
    return didRead;
}

bool PluckyInterfaceTcpPort::availableForWrite(size_t len) {
    // Note this simple loop biases our checking to the lower-index interfaces.
    // However because writeAll() visits and writes ALL interfaces in the group
    // (leaving it to the leaf interfaces to handle buffering, flushing, closing, etc)
    // this does not introduce a risk of blockage or starvation.
    for (uint16_t i=0; i<_numInterfaces; i++) {
        if (_interfaces[i]->availableForWrite(len)) {
            return true;
        }
    }
    return false;
}

bool PluckyInterfaceTcpPort::writeAll(const uint8_t *buf, size_t size) {
    bool didWrite = false;
    for (uint16_t i=0; i<_numInterfaces; i++) {
        didWrite = (_interfaces[i]->writeAll(buf, size) || didWrite);
    }
    return didWrite;
}

