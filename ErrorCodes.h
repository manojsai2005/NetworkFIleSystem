//
// Created by George Rahul on 16/11/24.
//

#ifndef NFS_OSN_ERRORCODES_H
#define NFS_OSN_ERRORCODES_H
#include <stddef.h>
// Define a struct for error details
typedef struct {
    int code;           // Error number
    const char *name;   // Error code name
    const char *message; // Error description
} Error;

// Define the error list
const Error errors[] = {
        {0, "ERR_NO_FILE_FOUND", "No file found"},
        {1, "ERR_NO_FOLDER_FOUND", "No folder found"},
        {2, "ERR_STORAGE_SERVER_DOWN", "Storage server is down"},
        {3, "ERR_NAMING_SERVER_UNREACHABLE", "Network is unreachable"},
        {4, "ERR_PERMISSION_DENIED", "Permission denied"},
        {5, "ERR_DISK_FULL", "Disk is full"},
        {6, "ERR_UNKNOWN_ERROR", "Unknown error"}
};

// Number of errors in the array
#define ERROR_COUNT (sizeof(errors) / sizeof(errors[0]))

// Function to print error details by code
void printErrorDetails(int errorCode) {
    for (int i = 0; i < ERROR_COUNT; i++) {
        if (errors[i].code == errorCode) {
            printf("Error Number: %d\n", errors[i].code);
            printf("Error Code: %s\n", errors[i].name);
            printf("Error Message: %s\n", errors[i].message);
            return;
        }
    }
    printf("Error: Invalid error code (%d)\n", errorCode);
}




#endif //NFS_OSN_ERRORCODES_H
