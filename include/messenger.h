#pragma once

extern char message_str[DEFAULT_BUFFER_SIZE + 1];
extern char receive_msg_str[DEFAULT_BUFFER_SIZE + 1];
extern char display_msg[DEFAULT_BUFFER_SIZE + 1];

void messenger_task_create();
