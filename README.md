# erdhe

XMPP Welcome Bot in C

Build and run:

``
make
``

Run the bot:

``
./xmpp_bot 2>&1
``

Remember to replace BOT_JID and BOT_PASSWORD with your actual credentials. If you're getting connection errors, you might need to verify:

The server's TLS certificate is valid
The server allows client connections
Your network allows outbound XMPP traffic

-------------

Key changes made to the code:

Added a configurable welcome message with a default template that includes the user's nickname: DEFAULT_WELCOME_MSG "Welcome to the room, %s!"
Added functionality to extract nicknames from full JIDs using the get_nickname function
Implemented a message handler to process room messages and check for the command to update the welcome message (.erdha wm)
Added logic to check if the user trying to change the welcome message is an admin or owner
Modified the presence handler to:

Not send welcome messages to users already in the room when the bot joins
Use the configurable welcome message format with the user's nickname


Added a separate function send_welcome_message to format and send the welcome message

To use the updated bot:

The bot will automatically join the room and not send welcome messages to existing users
When new users join, they'll receive a whispered welcome message that includes their nickname
Room admins or owners can change the welcome message using the command: .erdha wm "New welcome message"

The message can include %s which will be replaced with the joining user's nickname



Note that the welcome message has a size limit of 511 characters to prevent buffer overflows.
