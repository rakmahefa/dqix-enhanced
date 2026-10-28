/* Discord Rich Presence: "Playing Dragon Quest IX" on the player's profile (discord.c). */
#pragma once
void discord_set_app_id(const char* id);   /* (settings.ini's discord_app_id, for tests) */
int discord_available(void);               /* the project's Discord application is known */
void discord_enable(int on);
void discord_activity(const char* details, const char* state);
void discord_quit(void);
