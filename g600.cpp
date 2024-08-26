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
#define KEY_STATE_SIZE 256 

class G600Controller {
public:
    G600Controller(const char *downCmds[], const char *upCmds[])
        : fd(-1) {
        memset(keyStates, 0, sizeof(keyStates));

        for (int i = 0; i < KEY_STATE_SIZE; ++i) {
            downCommands[i] = downCmds[i];
            upCommands[i] = upCmds[i];
        }

        printIntro();
    }

    ~G600Controller() {
        if (fd >= 0) {
            close(fd);
        }
    }

    int run() {
        char path[MAX_PATH_LENGTH];
        int find_error = find_g600(path);
        if (find_error) {
            handleError(find_error);
            return 1;
        }

        if (initializeDevice(path)) {
            return 1;
        }

        printf("G600 controller started successfully.\n\n");

        return processEvents();
    }

private:
    const char kDir[MAX_PATH_LENGTH] = "/dev/input/by-id/";
    const char kPrefix[MAX_PATH_LENGTH] = "usb-Logitech_Gaming_Mouse_G600_";
    const char kSuffix[MAX_PATH_LENGTH] = "-if01-event-kbd";

    const char *downCommands[KEY_STATE_SIZE];
    const char *upCommands[KEY_STATE_SIZE];

    int fd;
    int keyStates[KEY_STATE_SIZE];

    void printIntro() const {
        printf("Starting G600 Linux controller.\n\n");
        printf("It's a good idea to configure G600 with Logitech Gaming Software before running this program:\n");
        printf(" - Assign left, right, middle mouse button and vertical mouse wheel to their normal functions\n");
        printf(" - Assign the G-Shift button to \"G-Shift\"\n");
        printf(" - Assign all other keys (including horizontal mouse wheel) to arbitrary (unique) keyboard keys\n");
        printf("\n");
    }

    int starts_with(const char* haystack, const char* prefix) const {
        size_t prefix_length = strlen(prefix);
        size_t haystack_length = strlen(haystack);
        return haystack_length >= prefix_length && strncmp(haystack, prefix, prefix_length) == 0;
    }

    int ends_with(const char* haystack, const char* suffix) const {
        size_t suffix_length = strlen(suffix);
        size_t haystack_length = strlen(haystack);
        return haystack_length >= suffix_length && strncmp(haystack + (haystack_length - suffix_length), suffix, suffix_length) == 0;
    }

    int find_g600(char *path) const {
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

    void handleError(int find_error) const {
        printf("Error: Couldn't find G600 input device.\n");
        if (find_error == 1) {
            printf("Suggestion: Check whether the directory %s exists and fix it by editing the source code.\n", kDir);
        } else if (find_error == 2) {
            printf("Suggestion: Check whether a device with the prefix %s exists in %s and fix it by editing the source code.\n", kPrefix, kDir);
        }
        printf("Suggestion: Maybe a permission is missing. Try running this program with sudo.\n");
    }

    int initializeDevice(const char *path) {
        fd = open(path, O_RDONLY);
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

        return 0;
    }

    int processEvents() {
        struct input_event events[MAX_EVENTS];

        while (true) {
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
                    printf("Scan code: %d\n", scancode);

                    if (pressed && keyStates[scancode] == 0) {
                        keyStates[scancode] = 1;
                        const char *cmdToRun = downCommands[scancode];
                        if (cmdToRun && strlen(cmdToRun) > 0) {
                            executeCommand(cmdToRun);
                        } 
                    } else if (!pressed && keyStates[scancode] == 1) {
                        keyStates[scancode] = 0;
                        const char *cmdToRun = upCommands[scancode];
                        if (cmdToRun && strlen(cmdToRun) > 0) {
                            executeCommand(cmdToRun);
                        }
                    }
                }
            }
        }

        return 0;
    }

    void executeCommand(const char *cmd) const {
        if (cmd && strlen(cmd) > 0) {
            printf("Executing: \"%s\"\n", cmd);
            int result = system(cmd);
            if (result == -1) {
                perror("Error executing command\n");
            }
        }
    }
};

int main() {
    const char *downCommands[KEY_STATE_SIZE] = {NULL};
    const char *upCommands[KEY_STATE_SIZE] = {NULL};

    downCommands[79] = "xdotool keydown ctrl";   // G9
    downCommands[80] = "";    // G10
    downCommands[81] = "xdotool keydown alt";    // G11
    downCommands[75] = "xdotool keydown shift";  // G12
    downCommands[76] = "";  // G13
    downCommands[77] = "";  // G14
    downCommands[71] = "";  // G15
    downCommands[72] = "";  // G16
    downCommands[73] = "";  // G17
    downCommands[82] = "";  // G18
    downCommands[74] = "";  // G19
    downCommands[78] = "";  // G20

    upCommands[79] = "xdotool keyup ctrl";       // G9
    upCommands[80] = "";    // G10
    upCommands[81] = "xdotool keyup alt";        // G11
    upCommands[75] = "xdotool keyup shift";      // G12
    upCommands[76] = "";  // G13
    upCommands[77] = "";  // G14
    upCommands[71] = "";  // G15
    upCommands[72] = "";  // G16
    upCommands[73] = "";  // G17
    upCommands[82] = "";  // G18
    upCommands[74] = "";  // G19
    upCommands[78] = "";  // G20

    G600Controller controller(downCommands, upCommands);
    return controller.run();
}
