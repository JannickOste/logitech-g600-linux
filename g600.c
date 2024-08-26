#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_EVENTS 64
#define MAX_PATH_LENGTH 1024
#define KEY_STATE_SIZE 256 // States added to avoid command spam

const char kDir[] = "/dev/input/by-id/";
const char kPrefix[] = "usb-Logitech_Gaming_Mouse_G600_";
const char kSuffix[] = "-if01-event-kbd";

const char *downCommands[] = {
    [4] = "",    // scroll left
    [5] = "xdotool key Page_Down",  // scroll right
    [79] = "xdotool keydown ctrl",    // G9
    [81] = "xdotool keydown alt",         // G11
    [75] = "xdotool keydown shift",       // G12
};
const char *upCommands[] = {
    [79] = "xdotool keyup ctrl",       // G9
    [81] = "xdotool keyup alt",        // G11
    [75] = "xdotool keyup shift",      // G12
};

int starts_with(const char* haystack, const char* prefix) {
    size_t prefix_length = strlen(prefix);
    size_t haystack_length = strlen(haystack);
    return haystack_length >= prefix_length && strncmp(haystack, prefix, prefix_length) == 0;
}
int ends_with(const char* haystack, const char* suffix) {
    size_t suffix_length = strlen(suffix);
    size_t haystack_length = strlen(haystack);
    return haystack_length >= suffix_length && strncmp(haystack + (haystack_length - suffix_length), suffix, suffix_length) == 0;
}

int find_g600(char *path) {
    DIR *dir = opendir(kDir);
    if (!dir) {
        perror("Error opening directory");
        return 1;
    }

    struct dirent *ent;
    while ((ent = readdir(dir))) {
        if (starts_with(ent->d_name, kPrefix) && ends_with(ent->d_name, kSuffix)) {
            snprintf(path, MAX_PATH_LENGTH, "%s%s", kDir, ent->d_name);
            printf("Full path is %s\n", path);
            closedir(dir);
            return 0;
        }
    }

    closedir(dir);
    return 2;
}


int main() {
    printf("Starting G600 Linux controller.\n\n");
    printf("It's a good idea to configure G600 with Logitech Gaming Software before running this program:\n");
    printf(" - Assign left, right, middle mouse button and vertical mouse wheel to their normal functions\n");
    printf(" - Assign the G-Shift button to \"G-Shift\"\n");
    printf(" - Assign all other keys (including horizontal mouse wheel) to arbitrary (unique) keyboard keys\n");
    printf("\n");

    char path[MAX_PATH_LENGTH];
    int find_error = find_g600(path);
    if (find_error) {
        printf("Error: Couldn't find G600 input device.\n");
        if (find_error == 1) {
            printf("Suggestion: Check whether the directory %s exists and fix it by editing \"g600.c\".\n", kDir);
        } else if (find_error == 2) {
            printf("Suggestion: Check whether a device with the prefix %s exists in %s and fix it by editing \"g600.c\".\n", kPrefix, kDir);
        }
        printf("Suggestion: Maybe a permission is missing. Try running this program with sudo.\n");
        return 1;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("Error opening device");
        printf("Suggestion: Maybe a permission is missing. Try running this program with sudo.\n");
        return 1;
    }

    if (ioctl(fd, EVIOCGRAB, 1) < 0) {
        perror("Error grabbing device");
        close(fd);
        return 1;
    }

    printf("G600 controller started successfully.\n\n");

    struct input_event events[MAX_EVENTS];
    int keyStates[KEY_STATE_SIZE] = {0}; 

    while (1) {
        ssize_t n = read(fd, events, sizeof(events));
        if (n <= 0) {
            perror("Error reading device");
            close(fd);
            return 2;
        }

        if (n < sizeof(struct input_event) * 2) continue;

        for (size_t i = 0; i < n / sizeof(struct input_event); ++i) {
            if (events[i].type == EV_KEY) {
                int pressed = events[i].value;
                int scancode = events[i].code;

                // Check if the key state has changed
                if (pressed && keyStates[scancode] == 0) {
                    // Key pressed
                    keyStates[scancode] = 1;
                    const char *cmdToRun = downCommands[scancode];
                    if (cmdToRun && strlen(cmdToRun) > 0) {
                        printf("Executing: \"%s\"\n", cmdToRun);
                        int result = system(cmdToRun);
                        if (result == -1) {
                            perror("Error executing command");
                        }
                        printf("\n");
                    }
                } else if (!pressed && keyStates[scancode] == 1) {
                    // Key released
                    keyStates[scancode] = 0;
                    const char *cmdToRun = upCommands[scancode];
                    if (cmdToRun && strlen(cmdToRun) > 0) {
                        printf("Executing: \"%s\"\n", cmdToRun);
                        int result = system(cmdToRun);
                        if (result == -1) {
                            perror("Error executing command");
                        }
                        printf("\n");
                    }
                }
            }
        }
    }

    close(fd);
    return 0;
}
