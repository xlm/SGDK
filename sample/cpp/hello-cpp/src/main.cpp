#include <new>
#include <genesis.h>
// Also works with the bundled rescomp.jar, before generated C++ guards.
extern "C" {
#include "resources.h"
}

static u16 constructed;
static u16 destroyed;

template<typename T>
constexpr T twice(T value)
{
    return value + value;
}

// A virtual destructor lets delete through the base class clean up the object.
class Message
{
public:
    virtual ~Message() { destroyed++; }
    virtual const char* text() const = 0;
};

class Greeting : public Message
{
    u16 number;

public:
    explicit Greeting(u16 value) : number(value) { constructed++; }
    ~Greeting() override {}
    const char* text() const override { return "Hello from C++!"; }
    u16 value() const { return number; }
};

// This constructor runs before main. Its destructor never runs on shutdown.
static Greeting global(twice<u16>(21));

static Greeting& localGreeting()
{
    // Initialized on the first call, without thread-safe guard machinery.
    static Greeting local(7);
    return local;
}

int main(bool hardReset)
{
    constexpr u16 row = 4;
    auto status = [](bool ready) { return ready ? "C++ runtime OK" : "C++ runtime failed"; };

    Message* message = new Greeting(1);
    VDP_drawText(message->text(), 2, row);
    delete message;

    // Array delete also runs the destructor for every element.
    auto* values = new Greeting[2]{Greeting(3), Greeting(4)};
    const bool arrayOK = values[1].value() == 4;
    delete[] values;

    auto* optional = new (std::nothrow) Greeting(2);
    const bool allocated = optional != nullptr;
    delete optional;

    const bool localOK = &localGreeting() == &localGreeting() && localGreeting().value() == 7;
    const bool ready = global.value() == 42 && localOK && arrayOK && allocated &&
        constructed == 6 && destroyed == 4 && greeting[0] == 'S';
    VDP_drawText(status(ready), 2, row + 2);

    while (true)
        SYS_doVBlankProcess();
}
