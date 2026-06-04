#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[])
{
    openlog("writer", LOG_PID, LOG_USER);

    if (argc != 3)
    {
        syslog(LOG_ERR, "Invalid number of arguments. Expected 2 arguments: writefile and writestr");
        closelog();
        return 1;
    }

    const char *writefile = argv[1];
    const char *writestr = argv[2];

    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);

    FILE *file = fopen(writefile, "w");
    if (file == NULL)
    {
        syslog(LOG_ERR, "Failed to open file %s: %s", writefile, strerror(errno));
        closelog();
        return 1;
    }

    if (fprintf(file, "%s", writestr) < 0)
    {
        syslog(LOG_ERR, "Failed to write to file %s: %s", writefile, strerror(errno));
        fclose(file);
        closelog();
        return 1;
    }

    if (fclose(file) != 0)
    {
        syslog(LOG_ERR, "Failed to close file %s: %s", writefile, strerror(errno));
        closelog();
        return 1;
    }

    closelog();
    return 0;
}
