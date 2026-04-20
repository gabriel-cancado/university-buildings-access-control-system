#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "../include/common.h"

typedef struct {
    int soc;
    int id;
} server_connection_info;

void usage_exit(int argc, char** argv) {
    char* progam_name = argv[0];
    printf("usage: %s <servers IP> <users server port> <loc server port> <loc_id>\n", progam_name);
    printf("example: %s 127.0.0.1 50000 6000 2\n", progam_name);

    exit(EXIT_FAILURE);
}

server_connection_info open_connection(char* server_addr_str, char* server_port_str, int loc_id) {
    struct sockaddr_storage storage;
    int success = addr_parse(server_addr_str, server_port_str, &storage);
    if (success == -1) log_exit("Invalid server address. Valid types are IPv4 and IPv6");

    int soc = socket(storage.ss_family, SOCK_STREAM, 0);
    if (soc == -1) log_exit("Error creating socket");

    success = connect(soc, (struct sockaddr *)&storage, sizeof(storage));
    if (success == -1) log_exit("Could not connect to server");

    message msg = {
        .code = REQ_CONN,
        .payload = {
            .loc_id = loc_id
        }
    };

    message response = send_message(soc, msg, true);
    if (response.code == ERROR) error_exit(response.payload.description);

    server_connection_info connection_info = {
        .id = response.payload.client_id,
        .soc = soc
    };
    return connection_info;
}

int kill_connection(int server_soc, int client_id, char* server_name) {
    message disconnect_request = {
        .code = REQ_DISC,
        .payload = {
            .client_id = client_id
        }
    };

    message response = send_message(server_soc, disconnect_request, true);

    if (response.code == ERROR) {
        printf("%s", get_message_description(ERROR, response.payload.description_code));
        return -1;
    }

    if (response.code != OK) log_exit("Invalid response code");

    char* msg_description = get_message_description(OK, response.payload.description_code);
    printf("%s %s\n", server_name, msg_description);

    close(server_soc);
    return 0;
}

void command_kill(server_connection_info users_server_connection_info, server_connection_info loc_server_connection_info) {
    int us_success = kill_connection(users_server_connection_info.soc, users_server_connection_info.id, "SU");
    int ls_success = kill_connection(loc_server_connection_info.soc, loc_server_connection_info.id, "SL");

    if (us_success == 0 && ls_success == 0) exit(0);
}

void command_add(server_connection_info users_server_connection_info, int user_id, int is_special) {
    message request = {
        .code = REQ_USRADD,
        .payload = {
            .user_id = user_id,
            .is_special = is_special
        }
    };

    message response = send_message(users_server_connection_info.soc, request, true);

    if (response.code != OK && response.code != ERROR) log_exit("Invalid response");

    if (response.code == OK && response.payload.description_code == 3) {
        printf("User updated: %d\n", user_id);
        return;
    }

    if (response.code == OK && response.payload.description_code == 2) {
        printf("New user added: %d\n", user_id);
        return;
    }

    printf("%s\n", get_message_description(ERROR, response.payload.description_code));
}

void command_in_out(server_connection_info users_server_connection_info, int user_id, int direction) {
    message request = {
        .code = REQ_USRACCESS,
        .payload = {
            .user_id = user_id,
            .direction = direction
        }
    };

    message response = send_message(users_server_connection_info.soc, request, true);
    if (response.code == ERROR) {
        printf("%s\n", get_message_description(ERROR, response.payload.description_code));
        return;
    }

    printf("Ok. Last location: %d\n", response.payload.loc_id);
}

void handle_input(
    char* input,
    server_connection_info users_server_connection_info,
    server_connection_info loc_server_connection_info
) {
    input = strtok(input, "\n");
    char* command = strtok(input, " ");

    if (strcmp(command, "kill") == 0) {
        command_kill(users_server_connection_info, loc_server_connection_info);
        return;
    }

    if (strcmp(command, "add") == 0) {
        int user_id = atoi(strtok(NULL, " "));
        int is_special = atoi(strtok(NULL, " "));
        command_add(users_server_connection_info, user_id, is_special);
        return;
    }

    if (strcmp(command, "in") == 0) {
        int user_id = atoi(strtok(NULL, " "));
        command_in_out(users_server_connection_info, user_id, DIRECTION_IN);
        return;
    }
}

void main (int argc, char** argv) {
    if (argc < 5) usage_exit(argc, argv);

    int loc_id = atoi(argv[4]);
    if (loc_id < 0 || loc_id > 10) error_exit("Invalid argument");

    server_connection_info users_server_connection_info = open_connection(argv[1], argv[2], loc_id);
    printf("SU New ID: %d\n", users_server_connection_info.id);

    server_connection_info loc_server_connection_info = open_connection(argv[1], argv[3], loc_id);
    printf("SL New ID: %d\n", loc_server_connection_info.id);

    while(1) {
        char input[1024];
        fgets(input, sizeof(input), stdin);

        handle_input(input, users_server_connection_info, loc_server_connection_info);
    };
}