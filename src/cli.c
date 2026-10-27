#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/run/argon-v5-oled.sock"

static void print_usage(const char *prog) {
    printf("Argon ONE V5 OLED Control CLI\n");
    printf("Usage: %s <command>\n\n", prog);
    printf("Commands:\n");
    printf("  sensors [--json]         Scan and report all standard & expansion hardware sensors\n");
    printf("  next                     Switch to next screen\n");
    printf("  prev                     Switch to previous screen\n");
    printf("  toggle                   Toggle display on / sleep\n");
    printf("  on                       Wake up display\n");
    printf("  off                      Put display to sleep\n");
    printf("  dim                      Toggle display dimming (normal / dim)\n");
    printf("  brightness <0-255>       Set active OLED brightness contrast\n");
    printf("  screen <N>               Jump to screen number (0=CPU, 1=NVMe/RAM, 2=Net, 3=Clock)\n");
    printf("  variant <target> <a|b|c> Set card layout variant (cpu, mem, net, clock, all)\n");
    printf("  set <key> <val>          Update configuration setting dynamically and persist\n");
    printf("  reload                   Reload /etc/argon-v5-oled.conf\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "sensors") == 0 || strcmp(argv[1], "discover") == 0) {
        const char *script_paths[] = {
            "/usr/bin/argon-hw-discover",
            "/usr/bin/argon_hw_discovery.py",
            "./argon_hw_discovery.py",
            "/home/playx/Downloads/argon-v5-oled/argon_hw_discovery.py",
            NULL
        };
        const char *found_path = NULL;
        for (int i = 0; script_paths[i]; i++) {
            if (access(script_paths[i], X_OK) == 0 || access(script_paths[i], R_OK) == 0) {
                found_path = script_paths[i];
                break;
            }
        }
        if (!found_path) {
            fprintf(stderr, "Error: argon-hw-discover engine not found.\n");
            return 1;
        }
        char exec_cmd[256];
        if (argc >= 3 && strcmp(argv[2], "--json") == 0) {
            snprintf(exec_cmd, sizeof(exec_cmd), "%s --json", found_path);
        } else {
            snprintf(exec_cmd, sizeof(exec_cmd), "%s", found_path);
        }
        return system(exec_cmd);
    }

    char cmd[128];
    if (strcmp(argv[1], "screen") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: missing screen number (0-3)\n");
            return 1;
        }
        snprintf(cmd, sizeof(cmd), "screen:%s", argv[2]);
    } else if (strcmp(argv[1], "variant") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: usage is 'variant <cpu|mem|net|clock> <a|b|c>'\n");
            return 1;
        }
        snprintf(cmd, sizeof(cmd), "variant:%s:%s", argv[2], argv[3]);
    } else if (strcmp(argv[1], "set") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: usage is 'set <key> <value>'\n");
            return 1;
        }
        snprintf(cmd, sizeof(cmd), "set:%s:%s", argv[2], argv[3]);
    } else if (strcmp(argv[1], "brightness") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: missing brightness value (0-255)\n");
            return 1;
        }
        snprintf(cmd, sizeof(cmd), "brightness:%s", argv[2]);
    } else if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    } else {
        snprintf(cmd, sizeof(cmd), "%s", argv[1]);
    }

    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (sendto(fd, cmd, strlen(cmd), 0, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Failed to send command to argon-v5-oled daemon (is it running?)");
        close(fd);
        return 1;
    }

    close(fd);
    printf("Command '%s' sent successfully.\n", cmd);
    return 0;
}
