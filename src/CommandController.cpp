// Implementation of CommandController that implements ICommandController
#include "ICommandController.h"
#include <functional>
#include <string>
#include <Arduino.h>

class CommandController : public ICommandController {
public:
    using Reader = std::function<bool(std::string& out)>;
    using Sender = std::function<void(const std::string& cmd)>;

    CommandController(Reader reader, Sender sender = nullptr)
        : reader_(reader), sender_(sender) {}

    void sendCommand(const std::string& cmd) override {
        if (sender_) sender_(cmd);
    }

    std::string receiveCommand() override {
        std::string cmd;
        // Loop until reader_ reports a command is available. Returns as
        // soon as a command is received, stopping further listening.
        while (true) {
            if (reader_ && reader_(cmd)) {
                return cmd;
            }
                // avoid busy-waiting on embedded platforms
                delay(10);
        }
    }

private:
    Reader reader_;
    Sender sender_;
};

// Factory implementation returning interface pointer
ICommandController* createCommandController(
    const std::function<bool(std::string&)>& reader,
    const std::function<void(const std::string&)>& sender)
{
    return new CommandController(reader, sender);
}
