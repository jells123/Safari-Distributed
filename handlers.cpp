#include "handlers.h"

std::vector<messageHandler> handlers;

void addMessageHandler(MsgType type, void (*handler)(packet*, int)) {
    messageHandler newHandler = {
        type,
        handler
    };

    handlers.push_back(newHandler);
    return;
}
