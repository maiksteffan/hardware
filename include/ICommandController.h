// Interface for a command controller
#ifndef ICOMMANDCONTROLLER_H
#define ICOMMANDCONTROLLER_H

#include <string>
#include <functional>

// Factory: create an ICommandController using provided reader/sender callables.
// Reader: signature `bool(std::string& out)` should return true and set `out`
// when a command is available. Sender: `void(const std::string&)`.
class ICommandController;
ICommandController* createCommandController(const std::function<bool(std::string&)>& reader,
                                           const std::function<void(const std::string&)>& sender = nullptr);

class ICommandController {
public:
    virtual ~ICommandController() {}

    // Send a command (e.g. to a device or transport)
    virtual void sendCommand(const std::string& cmd) = 0;

    // Receive a command. This call should block (or loop) until a command
    // has been received and then return it. Implementations must stop
    // listening as soon as a command is available.
    virtual std::string receiveCommand() = 0;
};

#endif // ICOMMANDCONTROLLER_H
