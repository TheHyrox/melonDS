#include "RetroAchievements.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include <QByteArray>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QUrl>

#include "rc_client.h"
#include "rc_consoles.h"
#include "rc_error.h"

#include "EmuInstance.h"
#include "NDS.h"
#include "Platform.h"
#include "version.h"

using melonDS::Platform::Log;
using melonDS::Platform::LogLevel;

namespace
{
QString gUser;
QString gToken;
bool gHardcore = true;

// Nintendo DS main RAM exposed to achievements (RA address 0 == 0x02000000).
constexpr uint32_t kRAMSize = 0x400000;
}

namespace RetroAchievementsConfig
{
void Set(const QString& user, const QString& token, bool hardcore)
{
    gUser = user.trimmed();
    gToken = token.trimmed();
    gHardcore = hardcore;
}

bool IsConfigured()
{
    return !gUser.isEmpty() && !gToken.isEmpty();
}
}

// ── construction ───────────────────────────────────────────────────────────

RetroAchievements::RetroAchievements(EmuInstance* inst) : QObject(nullptr), emu(inst)
{
    if (!RetroAchievementsConfig::IsConfigured())
        return;

    nam = new QNetworkAccessManager(this);

    client = rc_client_create(&RetroAchievements::readMemory, &RetroAchievements::serverCall);
    rc_client_set_userdata(client, this);
    rc_client_set_event_handler(client, &RetroAchievements::eventHandler);
    rc_client_set_hardcore_enabled(client, gHardcore ? 1 : 0);
    rc_client_set_allow_background_memory_reads(client, 0);
    rc_client_enable_logging(client, RC_CLIENT_LOG_LEVEL_WARN, [](const char* msg, const rc_client_t*) {
        Log(LogLevel::Info, "[RetroAchievements] %s\n", msg);
    });

    loginPending = true;
    rc_client_begin_login_with_token(client, gUser.toUtf8().constData(), gToken.toUtf8().constData(),
                                     &RetroAchievements::loginCallback, this);
}

RetroAchievements::~RetroAchievements()
{
    // Destroy the network context first so in-flight replies cannot call back into a dead client.
    delete nam;
    nam = nullptr;

    if (client)
    {
        rc_client_destroy(client);
        client = nullptr;
    }
}

bool RetroAchievements::isLoggedIn() const
{
    return client && rc_client_get_user_info(client) != nullptr;
}

bool RetroAchievements::hardcoreActive() const
{
    return isLoggedIn() && rc_client_get_hardcore_enabled(client);
}

void RetroAchievements::osd(const char* fmt, ...)
{
    char msg[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    emu->osdAddMessage(0, "%s", msg);
}

// ── game lifecycle ─────────────────────────────────────────────────────────

void RetroAchievements::loadGame(const uint8_t* data, uint32_t len, const std::string& name)
{
    if (!client || loginFailed)
        return;

    unloadGame();
    pendingRom.assign(data, data + len);
    pendingName = name;

    if (!loginPending && isLoggedIn())
        startLoad();
    // otherwise startLoad() runs from loginCallback
}

void RetroAchievements::startLoad()
{
    if (pendingRom.empty())
        return;

    rc_client_begin_identify_and_load_game(client, RC_CONSOLE_NINTENDO_DS, pendingName.c_str(),
                                           pendingRom.data(), pendingRom.size(),
                                           &RetroAchievements::loadGameCallback, this);
    // rc_client hashes synchronously; the buffer is no longer needed.
    std::vector<uint8_t>().swap(pendingRom);
}

void RetroAchievements::unloadGame()
{
    std::vector<uint8_t>().swap(pendingRom);
    if (client && gameLoaded)
        rc_client_unload_game(client);
    gameLoaded = false;
}

void RetroAchievements::doFrame()
{
    if (client && gameLoaded)
        rc_client_do_frame(client);
}

void RetroAchievements::idle()
{
    if (client)
        rc_client_idle(client);
}

void RetroAchievements::reset()
{
    if (client && gameLoaded)
        rc_client_reset(client);
}

// ── rc_client callbacks ────────────────────────────────────────────────────

void RetroAchievements::loginCallback(int result, const char* error, rc_client_t* client, void* userdata)
{
    auto* self = static_cast<RetroAchievements*>(userdata);
    self->loginPending = false;

    if (result != RC_OK)
    {
        self->loginFailed = true;
        std::vector<uint8_t>().swap(self->pendingRom);
        self->osd("RetroAchievements login failed: %s", error ? error : "unknown error");
        return;
    }

    const rc_client_user_t* user = rc_client_get_user_info(client);
    self->osd("RetroAchievements: logged in as %s%s", user ? user->display_name : "?",
              rc_client_get_hardcore_enabled(client) ? " (Hardcore)" : "");

    if (!self->pendingRom.empty())
        self->startLoad();
}

void RetroAchievements::loadGameCallback(int result, const char* error, rc_client_t* client, void* userdata)
{
    auto* self = static_cast<RetroAchievements*>(userdata);

    if (result == RC_NO_GAME_LOADED)
    {
        self->osd("RetroAchievements: no achievements for this game");
        return;
    }
    if (result != RC_OK)
    {
        self->osd("RetroAchievements: could not load game (%s)", error ? error : "error");
        return;
    }

    self->gameLoaded = true;
    const rc_client_game_t* game = rc_client_get_game_info(client);
    rc_client_user_game_summary_t summary;
    rc_client_get_user_game_summary(client, &summary);

    self->osd("%s - %u/%u achievements unlocked", game ? game->title : "Game",
              summary.num_unlocked_achievements, summary.num_core_achievements);
}

void RetroAchievements::eventHandler(const rc_client_event_t* event, rc_client_t* client)
{
    auto* self = static_cast<RetroAchievements*>(rc_client_get_userdata(client));

    switch (event->type)
    {
    case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
        self->osd("Achievement Unlocked: %s (%u pts)", event->achievement->title, event->achievement->points);
        break;
    case RC_CLIENT_EVENT_GAME_COMPLETED:
        self->osd("Congratulations! You mastered this game.");
        break;
    case RC_CLIENT_EVENT_LEADERBOARD_STARTED:
        self->osd("Leaderboard attempt started: %s", event->leaderboard->title);
        break;
    case RC_CLIENT_EVENT_LEADERBOARD_FAILED:
        self->osd("Leaderboard attempt failed: %s", event->leaderboard->title);
        break;
    case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
        self->osd("Leaderboard submitted: %s", event->leaderboard->title);
        break;
    case RC_CLIENT_EVENT_SERVER_ERROR:
        self->osd("RetroAchievements server error: %s", event->server_error->error_message);
        break;
    case RC_CLIENT_EVENT_DISCONNECTED:
        self->osd("RetroAchievements: offline, unlocks will be sent when reconnected");
        break;
    case RC_CLIENT_EVENT_RECONNECTED:
        self->osd("RetroAchievements: reconnected");
        break;
    default:
        break;
    }
}

uint32_t RetroAchievements::readMemory(uint32_t address, uint8_t* buffer, uint32_t numBytes, rc_client_t* client)
{
    auto* self = static_cast<RetroAchievements*>(rc_client_get_userdata(client));
    melonDS::NDS* nds = self->emu->nds;
    if (!nds || address >= kRAMSize)
        return 0;

    if (address + numBytes > kRAMSize)
        numBytes = kRAMSize - address;

    // Main RAM is a power-of-two sized buffer; the mask keeps the access in range.
    const uint32_t mask = nds->MainRAMMask;
    for (uint32_t i = 0; i < numBytes; i++)
        buffer[i] = nds->MainRAM[(address + i) & mask];
    return numBytes;
}

// ── HTTP transport (Qt) ────────────────────────────────────────────────────

void RetroAchievements::serverCall(const rc_api_request_t* request,
                                   void (*callback)(const rc_api_server_response_t*, void*),
                                   void* userdata, rc_client_t* client)
{
    auto* self = static_cast<RetroAchievements*>(rc_client_get_userdata(client));
    self->submitRequest(request, callback, userdata);
}

void RetroAchievements::submitRequest(const rc_api_request_t* request,
                                      void (*callback)(const rc_api_server_response_t*, void*),
                                      void* userdata)
{
    const QString url = QString::fromUtf8(request->url);
    const QByteArray post = request->post_data ? QByteArray(request->post_data) : QByteArray();
    const QString contentType = request->content_type ? QString::fromUtf8(request->content_type) : QString();
    const bool isPost = request->post_data != nullptr;

    char agent[128] = {0};
    rc_client_get_user_agent_clause(client, agent, sizeof(agent));
    const QByteArray userAgent = QByteArray("melonDS/" MELONDS_VERSION " ") + agent;

    // QNetworkAccessManager lives in the thread that created this object (the GUI thread).
    QMetaObject::invokeMethod(this, [=]() {
        QNetworkRequest req{QUrl(url)};
        req.setHeader(QNetworkRequest::UserAgentHeader, userAgent);
        req.setTransferTimeout(30000);

        QNetworkReply* reply;
        if (isPost)
        {
            req.setHeader(QNetworkRequest::ContentTypeHeader,
                          contentType.isEmpty() ? QStringLiteral("application/x-www-form-urlencoded") : contentType);
            reply = nam->post(req, post);
        }
        else
        {
            reply = nam->get(req);
        }

        connect(reply, &QNetworkReply::finished, this, [reply, callback, userdata]() {
            const QByteArray body = reply->readAll();
            rc_api_server_response_t response;
            std::memset(&response, 0, sizeof(response));

            if (!reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid())
            {
                // Transport-level failure (offline, timeout, ...): let rc_client retry.
                const QByteArray msg = reply->errorString().toUtf8();
                response.body = msg.constData();
                response.body_length = msg.size();
                response.http_status_code = RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;
                callback(&response, userdata);
            }
            else
            {
                response.body = body.constData();
                response.body_length = body.size();
                response.http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                callback(&response, userdata);
            }
            reply->deleteLater();
        });
    }, Qt::QueuedConnection);
}
