#include <strophe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SERVER "xmppserver.net"
#define ROOM_JID "erdha@chat.hax.al"
#define BOT_NICKNAME "erdhe"
#define BOT_JID "account@xmppserver.net"
#define BOT_PASSWORD "thepasswordofjid"
#define CONFIG_FILE "config.txt"
#define DEFAULT_WELCOME_MSG "Welcome to the room, %s!"
#define CMD_PREFIX ".erdha wm"

/* Add UNUSED macro to suppress warnings */
#define UNUSED(x) (void)(x)

/* List to track users who have already received the whisper */
typedef struct UserList {
    char **users;
    int count;
    int size;
} UserList;

/* Global variables */
UserList userlist;
int bot_joined = 0;
char welcome_message[512];

/* Function prototypes */
void send_whisper(xmpp_conn_t * const conn, xmpp_ctx_t *ctx, const char *to_jid, const char *message);
void send_room_message(xmpp_conn_t * const conn, xmpp_ctx_t *ctx, const char *message);
int message_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata);
void load_welcome_message();
void save_welcome_message();

/* Logging function with timestamp */
void log_with_timestamp(const char *format, ...) {
    time_t now;
    struct tm *tm_info;
    char timestamp[26];
    va_list args;

    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(stderr, "[%s] ", timestamp);
    
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    
    fprintf(stderr, "\n");
}

/* User list functions */
void userlist_add(UserList *list, const char *jid) {
    if (list->count >= list->size) {
        list->size *= 2;
        list->users = realloc(list->users, list->size * sizeof(char *));
    }
    list->users[list->count] = strdup(jid);
    list->count++;
}

int userlist_contains(UserList *list, const char *jid) {
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->users[i], jid) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Extract nickname from full JID */
const char *get_nickname(const char *full_jid) {
    const char *slash = strrchr(full_jid, '/');
    return slash ? slash + 1 : full_jid;
}

/* Send a formatted welcome message */
void send_welcome_message(xmpp_conn_t * const conn, xmpp_ctx_t *ctx, const char *to_jid) {
    const char *nickname = get_nickname(to_jid);
    char formatted_msg[1024];
    snprintf(formatted_msg, sizeof(formatted_msg), welcome_message, nickname);
    send_whisper(conn, ctx, to_jid, formatted_msg);
}

/* Send a message to the room */
void send_room_message(xmpp_conn_t * const conn, xmpp_ctx_t *ctx, const char *message) {
    xmpp_stanza_t *msg = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(msg, "message");
    xmpp_stanza_set_type(msg, "groupchat");
    xmpp_stanza_set_attribute(msg, "to", ROOM_JID);

    xmpp_stanza_t *body = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(body, "body");
    
    xmpp_stanza_t *text = xmpp_stanza_new(ctx);
    xmpp_stanza_set_text(text, message);
    
    xmpp_stanza_add_child(body, text);
    xmpp_stanza_add_child(msg, body);
    
    xmpp_stanza_release(text);
    xmpp_stanza_release(body);

    xmpp_send(conn, msg);
    xmpp_stanza_release(msg);
    
    log_with_timestamp("Sent room message: %s", message);
}

/* Send a whisper message */
void send_whisper(xmpp_conn_t * const conn, xmpp_ctx_t *ctx, const char *to_jid, const char *message) {
    xmpp_stanza_t *msg = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(msg, "message");
    xmpp_stanza_set_type(msg, "chat");
    xmpp_stanza_set_attribute(msg, "to", to_jid);

    xmpp_stanza_t *body = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(body, "body");
    xmpp_stanza_t *text = xmpp_stanza_new(ctx);
    xmpp_stanza_set_text(text, message);
    xmpp_stanza_add_child(body, text);
    xmpp_stanza_add_child(msg, body);
    xmpp_stanza_release(text);
    xmpp_stanza_release(body);

    xmpp_send(conn, msg);
    xmpp_stanza_release(msg);

    log_with_timestamp("Sent whisper to %s: %s", to_jid, message);
}

/* Check if user is admin or owner */
int is_admin_or_owner(const char *affiliation) {
    return (strcmp(affiliation, "Owners") == 0 || strcmp(affiliation, "Administrators") == 0);
}

/* Handle presence stanzas */
int presence_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata) {
    const char *from = xmpp_stanza_get_attribute(stanza, "from");
    xmpp_ctx_t *ctx = (xmpp_ctx_t *)userdata;

    if (from && strstr(from, ROOM_JID)) {
        const char *resource = strchr(from, '/');
        if (resource && strcmp(resource + 1, BOT_NICKNAME) != 0) {
            if (bot_joined && !userlist_contains(&userlist, from)) {
                send_welcome_message(conn, ctx, from);
                userlist_add(&userlist, from);
            } else if (!bot_joined) {
                userlist_add(&userlist, from);
            }
        }
    }

    return 1;
}

/* Handle message stanzas */
int message_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata) {
    xmpp_ctx_t *ctx = (xmpp_ctx_t *)userdata;
    const char *type = xmpp_stanza_get_type(stanza);
    const char *from = xmpp_stanza_get_attribute(stanza, "from");

    if (type && strcmp(type, "groupchat") == 0) {
        xmpp_stanza_t *body = xmpp_stanza_get_child_by_name(stanza, "body");
        if (body) {
            char *message = xmpp_stanza_get_text(body);
            if (message && strncmp(message, CMD_PREFIX, strlen(CMD_PREFIX)) == 0) {
                /* Check if sender is admin/owner */
                xmpp_stanza_t *x = xmpp_stanza_get_child_by_ns(stanza, "http://jabber.org/protocol/muc#user");
                if (x) {
                    xmpp_stanza_t *item = xmpp_stanza_get_child_by_name(x, "item");
                    if (item) {
                        const char *affiliation = xmpp_stanza_get_attribute(item, "affiliation");
                        if (is_admin_or_owner(affiliation)) {
                            const char *nickname = get_nickname(from);
                            char *new_msg = message + strlen(CMD_PREFIX) + 1;
                            char old_msg[512];
                            strncpy(old_msg, welcome_message, sizeof(old_msg) - 1);
                            
                            strncpy(welcome_message, new_msg, sizeof(welcome_message) - 1);
                            welcome_message[sizeof(welcome_message) - 1] = '\0';
                            
                            /* Save the new message to the config file */
                            save_welcome_message();

                            /* Announce the change to the room */
                            char announcement[1024];
                            snprintf(announcement, sizeof(announcement), 
                                    "Welcome message changed by %s\nOld: %s\nNew: %s",
                                    nickname, old_msg, welcome_message);
                            send_room_message(conn, ctx, announcement);
                            
                            /* Log the change */
                            log_with_timestamp("Welcome message changed by %s from '%s' to '%s'", 
                                              nickname, old_msg, welcome_message);
                        }
                    }
                }
            }
            if (message) xmpp_free(ctx, message);
        }
    }
    return 1;
}

/* Load the welcome message from config file */
void load_welcome_message() {
    FILE *file = fopen(CONFIG_FILE, "r");
    if (file) {
        fgets(welcome_message, sizeof(welcome_message), file);
        fclose(file);
    } else {
        strncpy(welcome_message, DEFAULT_WELCOME_MSG, sizeof(welcome_message) - 1);
        welcome_message[sizeof(welcome_message) - 1] = '\0';
    }
}

/* Save the welcome message to the config file */
void save_welcome_message() {
    FILE *file = fopen(CONFIG_FILE, "w");
    if (file) {
        fprintf(file, "%s", welcome_message);
        fclose(file);
    }
}

/* Connection handler */
void conn_handler(xmpp_conn_t * const conn, xmpp_conn_event_t event, const int error, xmpp_stream_error_t * const stream_error, void * const userdata) {
    xmpp_ctx_t *ctx = (xmpp_ctx_t *)userdata;

    if (event == XMPP_CONN_CONNECT) {
        log_with_timestamp("Connected to server");
        
        xmpp_handler_add(conn, presence_handler, NULL, "presence", NULL, ctx);
        xmpp_handler_add(conn, message_handler, NULL, "message", NULL, ctx);
        
        xmpp_stanza_t *presence = xmpp_stanza_new(ctx);
        xmpp_stanza_set_name(presence, "presence");
        
        char to_buffer[512];
        snprintf(to_buffer, sizeof(to_buffer), "%s/%s", ROOM_JID, BOT_NICKNAME);
        xmpp_stanza_set_attribute(presence, "to", to_buffer);
        
        xmpp_stanza_t *x = xmpp_stanza_new(ctx);
        xmpp_stanza_set_name(x, "x");
        xmpp_stanza_set_ns(x, "http://jabber.org/protocol/muc");
        xmpp_stanza_add_child(presence, x);
        xmpp_stanza_release(x);
        
        xmpp_send(conn, presence);
        xmpp_stanza_release(presence);
        
        log_with_timestamp("Sent presence to join room: %s", to_buffer);
        bot_joined = 1;

    } else {
        if (error) {
            log_with_timestamp("Connection error: %d", error);
            if (stream_error) {
                log_with_timestamp("Stream error type: %d", stream_error->type);
            }
        }
        log_with_timestamp("Disconnected from server");
        xmpp_stop(ctx);
    }
}

/* Main function */
int main(void) {
    xmpp_ctx_t *ctx;
    xmpp_conn_t *conn;
    xmpp_log_t *log;

    /* Initialize userlist */
    userlist.size = 10;
    userlist.count = 0;
    userlist.users = malloc(userlist.size * sizeof(char *));
    
    /* Load the welcome message from config */
    load_welcome_message();

    xmpp_initialize();
    
    log = xmpp_get_default_logger(XMPP_LEVEL_DEBUG);
    ctx = xmpp_ctx_new(NULL, log);
    conn = xmpp_conn_new(ctx);
    
    xmpp_conn_set_jid(conn, BOT_JID);
    xmpp_conn_set_pass(conn, BOT_PASSWORD);
    xmpp_conn_set_flags(conn, XMPP_CONN_FLAG_MANDATORY_TLS);
    
    log_with_timestamp("Connecting to %s...", SERVER);
    
    if (xmpp_connect_client(conn, SERVER, 0, conn_handler, ctx) == XMPP_EOK) {
        log_with_timestamp("Running connection...");
        xmpp_run(ctx);
    } else {
        log_with_timestamp("Failed to start connection");
    }
    
    xmpp_conn_release(conn);
    xmpp_ctx_free(ctx);
    xmpp_shutdown();

    /* Free the userlist */
    for (int i = 0; i < userlist.count; i++) {
        free(userlist.users[i]);
    }
    free(userlist.users);

    return 0;
}
