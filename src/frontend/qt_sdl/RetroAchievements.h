/*
    RetroAchievements integration for the melonDS Qt frontend (via rcheevos' rc_client).

    Credentials are supplied on the command line (--ra-user / --ra-token / --ra-hardcore)
    and are intentionally never written to the config file.
*/

#ifndef RETROACHIEVEMENTS_H
#define RETROACHIEVEMENTS_H

#include <string>
#include <vector>
#include <cstdint>

#include <QObject>
#include <QString>

struct rc_client_t;
struct rc_client_event_t;
struct rc_api_request_t;
struct rc_api_server_response_t;
class QNetworkAccessManager;
class EmuInstance;

namespace RetroAchievementsConfig
{
// Set once from the command line, before any EmuInstance is created.
void Set(const QString& user, const QString& token, bool hardcore);
bool IsConfigured();
bool IsHardcoreRequested();
}

class RetroAchievements : public QObject
{
    Q_OBJECT

public:
    explicit RetroAchievements(EmuInstance* inst);
    ~RetroAchievements() override;

    // True when a user is logged in (credentials were provided and accepted).
    bool isLoggedIn() const;
    // True when hardcore rules (no savestates / cheats / slowmo / frame step) currently apply.
    bool hardcoreActive() const;

    // Identify and load the achievement set for a DS ROM (called from the emu/main thread
    // before the ROM buffer is handed over to the cart parser).
    void loadGame(const uint8_t* data, uint32_t len, const std::string& name);
    void unloadGame();

    // Emulation thread hooks.
    void doFrame();
    void idle();
    void reset();

    // Network entry point used by the rc_client server callback.
    void submitRequest(const rc_api_request_t* request, void (*callback)(const rc_api_server_response_t*, void*), void* userdata);

private:
    static uint32_t readMemory(uint32_t address, uint8_t* buffer, uint32_t numBytes, rc_client_t* client);
    static void serverCall(const rc_api_request_t* request, void (*callback)(const rc_api_server_response_t*, void*), void* userdata, rc_client_t* client);
    static void eventHandler(const rc_client_event_t* event, rc_client_t* client);
    static void loginCallback(int result, const char* error, rc_client_t* client, void* userdata);
    static void loadGameCallback(int result, const char* error, rc_client_t* client, void* userdata);

    void startLoad();
    void osd(const char* fmt, ...);

    EmuInstance* emu;
    rc_client_t* client = nullptr;
    QNetworkAccessManager* nam = nullptr;

    bool loginPending = false;
    bool loginFailed = false;
    bool gameLoaded = false;

    // ROM copy kept only while waiting for the login to complete.
    std::vector<uint8_t> pendingRom;
    std::string pendingName;
};

#endif // RETROACHIEVEMENTS_H
