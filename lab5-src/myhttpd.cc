const char * usage =
"                                                               \n"
"myhttpd-server:                                                \n"
"                                                               \n"
"To use it in type:                                             \n"
"                                                               \n"
"   myhttpd [-f|-t|-p] <port>                                   \n"
"                                                               \n"
"Where 1024 < port < 65536.                                     \n"
"                                                               \n"
"where <host> is the name of the machine where server is        \n"
"running. <port> is the port number you used when you run server\n"
"                                                               \n";

#include <stdio.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <dirent.h>
#include <string>
#include <dlfcn.h>
#include <link.h>
#include <errno.h>

int QueueLength = 5;
pthread_mutex_t mutex;
int port;
char password[256];

char const * firstPart_1 = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">\n<html>\n<head>\n<title>";
char const * firstPart_2 = "</title>\n</head>\n<body>";

char const * secondPart_1 = "</h1>\n<table>\n<tr><th valign=\"top\"><img src=\"/icons/blank.gif\" alt=\"[ICO]\"></th>";
char const * secondPart_2_ND = "<th><a href=\"?C=N;O=D\">Name</a></th>";
char const * secondPart_2_MA = "<th><a href=\"?C=M;O=A\">Last modified</a></th>";
char const * secondPart_2_SA = "<th><a href=\"?C=S;O=A\">Size</a></th>";
char const * secondPart_2_DA = "<th><a href=\"?C=D;O=A\">Description</a></th>";
char const * secondPart_3 = "</tr><tr><th colspan=\"5\"><hr></th></tr>\n";
char const * thirdPart_1 = "<tr><td valign=\"top\"><img src=\"";
char const * thirdPart_2 = "\" alt=\"[";
char const * thirdPart_3 = "]\"></td><td><a href=\"";
char const * thirdPart_4 = "\">";
char const * thirdPart_5 = "</a>";
char const * thirdPart_6 = " </td><td>&nbsp;</td><tr>\n";

char const * fourthPart = "<tr><th colspan=\"5\"><hr></th></tr>\n</table><address>Apache/2.4.18 (Ubuntu) Server at www.cs.purdue.edu Port 443</address>\n</body><html>";
char const * nbsp = "</td><td>&nbsp;";
char const * alignRight = " </td><td align=\"right\">";

char const * icon_unknown = "/icons/unknown.gif";
char const * icon_menu = "/icons/menu.gif";
char const * icon_back = "/icons/back.gif";
char const * icon_img = "icons/image2.gif";

char const * empty = "-"; 
// Processes time request
void processRequest( int socket );

// Helper function
bool endsWith(char * strA, char * strB);
void processRequestThread (int socket);
void poolSlave(int socket);
int sortNameA (const void *name1, const void *name2);
void writeHeader (int socket, char * contentType);
void writeFail (int socket, char * contentType);
void writeLink (int socket, char * path, char *name, char * fileType, char * parent);
void writeCGIHeader (int socket);
int sortNameA (const void *name1, const void *name2);
int sortModifiedTimeA (const void *name1, const void *name2);
int sortSizeA (const void *name1, const void *name2);
int sortNameD (const void *name1, const void *name2);
int sortModifiedTimeD (const void *name1, const void *name2);
int sortSizeD (const void *name1, const void *name2);

typedef void (*httprun)(int ssock, char * query_string);

extern "C" void sigIntHandler(int sig) {
  if (sig == SIGCHLD) {
  	pid_t pid = waitpid(-1, NULL, WNOHANG);
  }
}

// Statistics Variables
char * MYNAME = "Yechen Liu\n";
char * startTime;
int countRequest = 0;
double minimumTime = 100000000.0;
char slowestRequest [256];
double maximumTime = 0.0;
char fastestRequest [256];

int main(int argc, char** argv) {
    /*struct rlimit mem_limit = { .rlim_cur = 40960000, .rlim_max = 91280000 };
    struct rlimit cpu_limit = { .rlim_cur = 300, .rlim_max = 600 };
    struct rlimit nproc_limit = { .rlim_cur = 50, .rlim_max = 100 };
    if (setrlimit(RLIMIT_AS, &mem_limit)) {
        perror("Couldn't set memory limit\n");
    }
    if (setrlimit(RLIMIT_CPU, &cpu_limit)) {
        perror("Couldn't set CPU limit\n");
    }
    if (setrlimit(RLIMIT_NPROC, &nproc_limit)) {
        perror("Couldn't set NPROC limit\n");
    }*/

    // Get the server start time
    time_t rawtime;
    time(&rawtime);
    startTime = ctime(&rawtime);

    // Print usage if not enough arguments
    if ( argc < 2 || argc > 3 ) {
        fprintf( stderr, "%s", usage );
        exit( -1 );
    }
    
    port = 3334;
    char flag;
    // Get the port from the arguments
    if (argc == 2) {
        port = atoi(argv[1]);
        flag = 0;
    }

    if (argc == 3){
        port = atoi( argv[2] );
        flag = argv[1][1];
    }

    // Set the IP address and port for this server
    struct sockaddr_in serverIPAddress; 
    memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
    serverIPAddress.sin_family = AF_INET;
    serverIPAddress.sin_addr.s_addr = INADDR_ANY;
    serverIPAddress.sin_port = htons((u_short) port);

    // Allocate a socket
    int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
    if ( masterSocket < 0) {
        perror("socket");
        exit( -1 );
    }

    // Set socket options to reuse port. Otherwise we will
    // have to wait about 2 minutes before reusing the sae port number
    int optval = 1; 
    int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, (char *) &optval, sizeof( int ) );
    int error = bind( masterSocket, (struct sockaddr *) &serverIPAddress, sizeof(serverIPAddress) ); 
    if ( error ) {
        perror("bind");
        exit( -1 );
    }

    // Put socket in listening mode and set the 
    // size of the queue of unprocessed connections
    error = listen( masterSocket, QueueLength);
    if ( error ) {
        perror("listen");
        exit( -1 );
    }

    if (flag == 'p') {
            pthread_t tid[QueueLength]; 
            pthread_mutex_init(&mutex, NULL);
            for(int i=0; i<QueueLength;i++) {
                pthread_create(&tid[i], NULL, (void * (*) (void *)) poolSlave,
                            (void *)masterSocket);
            }
            pthread_join(tid[0], NULL);
        }
    else {
        while ( 1 ) {
            // Accept incoming connections
            struct sockaddr_in clientIPAddress;
            int alen = sizeof( clientIPAddress );
            int slaveSocket = accept( masterSocket, (struct sockaddr *)&clientIPAddress, (socklen_t*)&alen);

            if ( slaveSocket < 0 ) {
                perror( "accept" );
                exit( -1 );
            }
            // Process request.
            if (flag == 0) {
                processRequest( slaveSocket );
	        // Close socket
                close( slaveSocket );
	        }

            if (flag == 'f') {
                struct sigaction sa;
                sa.sa_handler = sigIntHandler;
                sigemptyset(&sa.sa_mask);
                sa.sa_flags = SA_RESTART;
                pid_t slave = fork();
                if(slave == 0) {
                    processRequest(slaveSocket);
                    close(slaveSocket);
                    exit(EXIT_SUCCESS);
                } else {
                    if (sigaction(SIGCHLD, &sa, NULL)) {
                        perror("sigaction");
                        exit(2);
                    }
                    close(slaveSocket);
	            }
            }

            if (flag == 't') {
                pthread_t t1;
                pthread_attr_t attr;
                pthread_attr_init(&attr);
                pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

                pthread_create(&t1, &attr, (void * (*) (void *)) processRequestThread,
                            (void *) slaveSocket);
                //pthread_join(t1, NULL);
            }

            //close( slaveSocket );
        }
        //waitpid(-1, NULL, 0);
    }
    return 0;
}

void poolSlave(int masterSocket) {
    while(1) {
        struct sockaddr_in clientIPAddress;
        int alen = sizeof( clientIPAddress );
        pthread_mutex_lock(&mutex);
        int slaveSocket = accept( masterSocket, (struct sockaddr *)&clientIPAddress, (socklen_t*)&alen);
        pthread_mutex_unlock(&mutex);

        if ( slaveSocket < 0 ) {
            perror( "accept" );
            exit( -1 );
        }

        processRequest(slaveSocket);
        close(slaveSocket);
    }
}

void processRequestThread (int socket) {
    processRequest(socket);
    close(socket);
}

bool cmpfunction (char * i, char * j) { return strcmp(i,j)<0; }


void processRequest( int socket ) {
    // Set statisticaal values
    countRequest ++;
    clock_t start_t, end_t, current_t;

    start_t = clock();

    // Buffer used to store the name received from the client
    const int MaxLen = 256;
    char currString[ MaxLen + 1 ];
    char docpath[MaxLen + 1];
    int length = 0;
    int n;

    // Currently character read
    unsigned char newChar;

    // Last character read
    unsigned char lastChar = 0;

    //
    // The client should send GET <sp> <Document Requested> <sp> HTTP/1.0 <crlf>
    // Read the name of the client character by character until a <CR><LF> is found.
    //
    int gotGet = 0;
    int gotCrlf = 0;
    int gotAuth = 0;
    int gotBasic = 0;
    int pswcmp = 0;
    int gotpsw = 0;
    char curr_password[256];
    curr_password[0]='\0';
    DIR * dir;

    while((n = read(socket, &newChar, sizeof(newChar)))>0) {
        printf("%c", newChar);
        if(newChar == ' ') {
            if (strncmp("GET", currString, 3) == 0) {
                gotGet = 1;
                bzero(currString, MaxLen);
                length = 0;
            }
            else if (strncmp("Authorization", currString, 13) == 0) {
                gotAuth = 1;
                bzero(currString, MaxLen);
                length = 0;
            }
            else if (strncmp("Basic", currString, 5) == 0 && gotAuth == 1) {
                gotBasic = 1;
                bzero(currString, MaxLen);
                length = 0;
                gotAuth = 0;
            }
            else if (gotGet == 1) {
                currString[length] = '\0';
                strcpy(docpath, currString);
                gotGet = 0;
            }
            else {
                gotAuth = 0;
                gotBasic = 0;
            }
        }
        else if(newChar == '\n' && lastChar == '\r') {
            if (gotBasic == 1) {
                if (strlen(password) == 0) {
                    strcpy(password, currString);
                    gotpsw = 1;
                }
                else if (strlen(curr_password) == 0)  {
                    strcpy(curr_password, currString);
                    gotpsw = 1;
                    pswcmp = strcmp(password, curr_password);
                }
                else {
                    pswcmp = strcmp(password, curr_password);
                }
                gotBasic = 0;
            }
            bzero(currString, MaxLen);
            length = 0;
            if (gotCrlf == 1) 
                break;
            gotCrlf = 1;
        }
        else{
            lastChar = newChar;
            if (newChar != '\r')
                gotCrlf = 0;
            currString[length] = newChar;
            length++;
        }
    }
    printf("password is %s\n", password);
    printf("current password is %s\n", curr_password);
    printf("compare result is %d\n", pswcmp);
    printf("got password? %d\n", gotpsw);
    printf("docpath is %s\n", docpath);

    //Map the document path to the real file 
    char cwd[256];
    char realPath[MaxLen];
    int parent = 0;
    getcwd(cwd, 256);
    if (strncmp("/icons", docpath, 6) == 0)
        sprintf(realPath,"%s/http-root-dir%s", cwd, docpath);
    else if (strncmp("/htdocs", docpath, 7) == 0)
        sprintf(realPath,"%s/http-root-dir%s", cwd, docpath);
    else if (strcmp("/", docpath) ==0 || strlen(docpath) == 0)
        sprintf(realPath,"%s/http-root-dir/htdocs/index.html", cwd);
    else if (strncmp("/cgi", docpath, 4) == 0){
        sprintf(realPath,"%s/http-root-dir%s", cwd, docpath);
    }
    else if (strstr(realPath, "..") != NULL )
        parent = 1;
    else
        sprintf(realPath,"%s/http-root-dir/htdocs%s", cwd, docpath);

    printf("realpath is %s\n", realPath);
  
    char * start;

    if (gotpsw == 0 || pswcmp != 0) {
            printf("Authentication failed\n");
            const char *realm = "\"myhttpd-cs252\"";
            write(socket, "HTTP/1.1 401 Unaothorized\r\n", 27);
            write(socket, "WWW-Authenticate: Basic realm=", 30);
            write(socket, realm, strlen(realm));
            write(socket, "\r\n\r\n", 4);
    }
    // CGI-BIN part
    else if (strstr(realPath, "cgi-bin")) {
        char exePath[128];
        start = strchr(realPath, '?');

        if (start) *start = '\0';
        strcpy(exePath, realPath);
        if (start) *start = '?';

        if(!open(exePath, O_RDONLY)) {
            writeFail(socket, "text/plain");
            perror("open(cgi)");
            exit(1);
        } 
        else {
            char ** args = (char **)malloc(sizeof(char *) * 2);
            args[0] = (char *)malloc(sizeof(char) * strlen(realPath));

            for (int i=0; i < strlen(realPath); i++) {
                args[0][i] = '\0';
            }

            args[1] = NULL;

            start = strchr(realPath, '?');

            if (start) {
                start++;
                strcpy(args[0], start);
            }

            writeCGIHeader(socket);

            //Loadable module
            if (endsWith(exePath, ".so")) {
                void * lib = dlopen(exePath, RTLD_LAZY);
                if (lib == NULL) {
                    perror("dlopen");
                }
    
                httprun httprunmod = (httprun)dlsym(lib, "httprun");
                if(httprunmod==NULL) {
                    perror("dlsym");
                }
                
                httprunmod(socket, args[0]);
            }
            // CGI-BIN module
            else {
                int tmpout = dup(1);
                dup2(socket, 1);
                close(socket);
    
                pid_t p = fork();
                if (p == 0) {
                    setenv("REQUEST_METHOD","GET",1);
                    setenv("QUERY_STRING", args[0],1);
                    execvp(exePath, args);
                    printf("I'm here in the child process\n");
                    exit(2);
                }
    
                dup2(tmpout,1);
                close(tmpout);
            }
            free(args[0]); free(args[1]);free(args);
        }

    }
    /* Statistics and Log pages
        https://localhost:<port>/stats
            * Name
            * Time the server has been up
            * Number of requests since the server started
            * Minimum service time and the URL request that took this time
            * Maximum service time and the URL request that took this time

         http://localhost:<port>/logs
            * Source host of the request
            * The directory requested
    */
    else if (endsWith(realPath, "stats") || endsWith(realPath, "stats/")) {
        writeHeader(socket, "text/plain");

        write(socket, MYNAME, strlen(MYNAME));

        write(socket, startTime, strlen(startTime));
        write(socket, "\n", 1);

        char nr[16];
        sprintf(nr, "%d", countRequest);
        write(socket, nr, strlen(nr));
        write(socket, "\n", 1);

        char mit[128];
        sprintf(mit, "%f", minimumTime);
        write(socket, mit, strlen(mit));
        write(socket, "\n", 1);

        write(socket, fastestRequest, strlen(fastestRequest));
        write(socket, "\n", 1);

        char mat[128];
        sprintf(mat, "%f", maximumTime);
        write(socket, mat, strlen(mat));
        write(socket, "\n", 1);

        write(socket, slowestRequest, strlen(slowestRequest));
        write(socket, "\n", 1);
    }
    else if((dir=opendir(realPath)) != NULL || endsWith(realPath, "?C=M;O=A") ||  
                endsWith(realPath, "?C=M;O=D") ||  endsWith(realPath, "?C=N;O=A") || 
                endsWith(realPath, "?C=N;O=D") ||  endsWith(realPath, "?C=S;O=A") ||  
                endsWith(realPath, "?C=S;O=D") ||endsWith(realPath, "?C=D;O=A") ||
                endsWith(realPath, "?C=D;O=D")) 
    {   
        printf("I'm in directory\n");
        char sortingMode = 'N';
        char sortingOrder = 'A';
        if (dir == NULL) {
            sortingMode = realPath[strlen(realPath) - 5];
            sortingOrder = realPath[strlen(realPath) - 1];
            realPath[strlen(realPath)-8] = '\0';
            docpath[strlen(docpath)-8] = '\0';
            dir=opendir(realPath);
        }
        
        printf("Sorting mode is %c, order is %c\n", sortingMode, sortingOrder);
        writeHeader(socket, "text/html");
        struct dirent * ent;
        char ** content = (char **)malloc(sizeof(char *)*128);
        int count_files = 0;
        printf("realpath before concatenate is %s\n", realPath);
        while((ent = readdir(dir))!= NULL) {
            if (strcmp(ent->d_name, ".") && strcmp(ent->d_name, "..")) {
                content[count_files] = (char *)malloc(sizeof(char)*(strlen(ent->d_name)+strlen(realPath)+5));
                content[count_files][0] = '\0';
                strcat(content[count_files], realPath);
                if (!endsWith(realPath, "/")) {
                    strcat(content[count_files], "/");
                }
                strcat(content[count_files], ent->d_name);
                printf("content[%d] is %s\n", count_files, content[count_files]);
                count_files++;
            }
        }
        // Sorting algorithm
        if (sortingMode == 'S') {
            if (sortingOrder == 'A') {
                qsort(content, count_files, sizeof(char *), sortSizeA);
            }
            if (sortingOrder == 'D') {
                printf("I'm here ?C=M;O=D\n");
                qsort(content, count_files, sizeof(char *), sortSizeD);
            }
        }
        /*sort by modified time*/
        else if (sortingMode == 'M') {
            if (sortingOrder == 'A') {
                qsort(content, count_files, sizeof(char *), sortModifiedTimeA);
            }
            if (sortingOrder == 'D') {
                qsort(content, count_files, sizeof(char *), sortModifiedTimeD);
            }
        }
        /*sort by name*/
        else {
            if (sortingOrder == 'D' && sortingMode == 'N') {
                qsort(content, count_files, sizeof(char *), sortNameD);
            }
            else {
                qsort(content, count_files, sizeof(char *), sortNameA);
            }
        }

        write(socket, firstPart_1, strlen(firstPart_1));
        write(socket, "Index of ", 9);
        write(socket, realPath, strlen(realPath));
        write(socket, firstPart_2, strlen(firstPart_2));
        write(socket, "Index of ", 9);
        write(socket, realPath, strlen(realPath));
        write(socket, secondPart_1, strlen(secondPart_1));
        write(socket, secondPart_2_ND, strlen(secondPart_2_ND));
        write(socket, secondPart_2_MA, strlen(secondPart_2_MA));
        write(socket, secondPart_2_SA, strlen(secondPart_2_SA));
        write(socket, secondPart_2_DA, strlen(secondPart_2_DA));
        write(socket, secondPart_3, strlen(secondPart_3));

        char parentPath[256];
        strcpy(parentPath, docpath);
        if (!endsWith(realPath, "/")) {
            parentPath[strlen(docpath)] ='/';
            parentPath[strlen(docpath)+2] ='.';
            parentPath[strlen(docpath)+3] ='\0';
        } else {
            parentPath[strlen(docpath)] ='.';
            parentPath[strlen(docpath)+2] ='\0';
        }
        parentPath[strlen(docpath)+1] ='.';
        writeLink(socket, parentPath, "Parent Directory", "DIR", docpath);

        for (int i=0; i<count_files; i++) {
            char name[128];
            int FileNameBegin = 0;
            for (int j=strlen(content[i])-1; j>=0; j--) {
                if (content[i][j] == '/') {
                    FileNameBegin = j;
                    break;
                }
            }

            for (int j=FileNameBegin+1; j<strlen(content[i]); j++) {
                name[j-FileNameBegin-1] = content[i][j];
                if (j==strlen(content[i])-1) {
                    name[j-FileNameBegin] = '\0';
                }
            }

            char * fileType;
            if (opendir(content[i]) != NULL) 
                fileType = "DIR";
            else
                fileType = "   ";
            printf("#[%d] is %s %s\n", i, name, realPath);
            writeLink(socket, content[i], name, fileType, docpath);
        }

        write(socket, fourthPart, strlen(fourthPart));

        for(int i=0; i<count_files; i++)
            free(content[i]);
        free(content);
        closedir(dir);
    } 
    else {
        char contentType[16];
        char html[8], gif[8], html1[8], gif1[8];
        strcpy(html, ".html"); strcpy(gif, ".gif");
        strcpy(html1, ".html/"); strcpy(gif1, ".gif/");
            
        if(endsWith(realPath, html) || endsWith(realPath, html1)){
            strcpy(contentType, "text/html");
        } 
        else if(endsWith(realPath, gif) || endsWith(realPath, gif1)){
            strcpy(contentType, "image/gif");
        }
        else if(endsWith(realPath, ".svg")) {
            strcpy(contentType, "image/svg+xml");
        }
        else
            strcpy(contentType, "text/plain");
        
        int fileinfo = open(realPath, O_RDWR);
        
        printf("Have I got authen? %d\n", gotAuth);
        printf("fileinfo is %d\n", fileinfo);
        printf("socket number is %d\n", socket);
        
        if ( fileinfo < 0 || parent == 1) {
            perror("open");
            writeFail(socket, contentType);
        } else {
            writeHeader(socket, contentType);
            char data[1000000];
            int count;
            while(count = read(fileinfo, data, 1000000)){
                if(write(socket, data, count) != count){
	                printf("%s\n", data);
	               perror("write");
	               break;
	               bzero(data, 1000000);
                }
            }
        }
        close(fileinfo);
    } 

    if (!endsWith(realPath, "stats") && !endsWith(realPath, "stats/")
        && !endsWith(realPath, "logs") && !endsWith(realPath, "logs/")) {
        end_t = clock();
        current_t = (double)(end_t - start_t);
    
        if (current_t < minimumTime) {
            minimumTime = current_t;
            strcpy(fastestRequest, realPath);
        }
    
        if (current_t > maximumTime) {
            maximumTime = current_t;
            strcpy(slowestRequest, realPath);
        }

        char sourceHost[1023];
        gethostname (sourceHost, 1023);
        strcat(sourceHost, ":");
    
        char convertPort[8];
        sprintf(convertPort, "%d", port);
        strcat(sourceHost, convertPort);
        strcat(sourceHost, realPath);
        strcat(sourceHost, "\n");
    
        FILE * f = fopen("/homes/liu2550/Desktop/cs252/lab5-src/http-root-dir/logs", "a+");
    
        fwrite(sourceHost, sizeof(char), strlen(sourceHost), f);
        fclose (f);
    }
}

bool endsWith(char * strA, char * strB) {
    int n = strlen(strB);
    int begin = strlen(strA) - n;
    if (begin < 0) return false;
    for (int i = 0; i<n; i++) {
        if (strA[begin+i] != strB[i])
            return false;
    }
    return true;
}

void writeLink (int socket, char * path, char *name, char * fileType, char * parent) {
    write(socket, thirdPart_1, strlen(thirdPart_1));

    char * gif = (char *)icon_unknown;
    char * alt = strdup(fileType);
    if (!strcmp(name, "Parent Directory")) 
        gif = (char *)icon_back;
    else if (!strcmp(fileType, "DIR"))
        gif = (char *)icon_menu;

    write(socket, gif, strlen(gif));
    write(socket, thirdPart_2, strlen(thirdPart_2));
    write(socket, alt, strlen(alt));
    write(socket, thirdPart_3, strlen(thirdPart_3));

    if (strcmp(name, "Parent Directory")) {
        if (!endsWith(parent, "/")) {
            char tmp[128];
            tmp[0] = '\0';
            strcat(tmp, parent);
            strcat(tmp, "/");
            strcat(tmp, name);
            write(socket, tmp, strlen(tmp));
        }
        else
            write(socket, name, strlen(name));
    } else 
        write(socket, path, strlen(path));

    write(socket, thirdPart_4, strlen(thirdPart_4));
    write(socket, name, strlen(name));
    write(socket, thirdPart_5, strlen(thirdPart_5));

    char m_time[20];
    m_time[0] = '\0';
    int s =0;
    printf("name is %s and result is %d\n", name, strcmp(name, "Parent Directory"));
    if (!strcmp(name, "Parent Directory")) {
        write(socket, nbsp, strlen(nbsp));
    } else {
        struct stat name_stat;
        stat(path, &name_stat);

        struct tm * timeInfo;
        timeInfo = localtime(&(name_stat.st_mtime));
        strftime(m_time, 20, "%F %H:%M", timeInfo);
        s = name_stat.st_size;
    }

    write(socket, alignRight, strlen(alignRight));
    write(socket, m_time, strlen(m_time));

    if (strcmp(name, "Parent Directory")) {
        write(socket,alignRight, strlen(alignRight));
        if (strcmp(fileType, "DIR")) {
            char _s[16];
            snprintf(_s, 16, "%d", s);
            write(socket, _s, strlen(_s));
        }
        else 
            write(socket, empty, strlen(empty));
    }
    write(socket, thirdPart_6, strlen(thirdPart_6));
}

void writeHeader (int socket, char * contentType) {
    write(socket, "HTTP/1.1 200 Document follows\r\n", 31);
    write(socket, "Server: CS 252 lab5\r\n", 21);
    write(socket, "Content-type: ", 14);
    write(socket, contentType, strlen(contentType));
    write(socket, "\r\n\r\n", 4);
}

void writeCGIHeader (int socket) {
    write(socket, "HTTP/1.1 200 Document follows\r\n", 31);
    write(socket, "Server: CS 252 lab5\r\n", 21);
}

void writeFail (int socket, char * contentType) {
    const char *notFound = "File not Found";
    write(socket, "HTTP/1.0 404FileNotFound\r\n", 26);
    write(socket, "Server: CS 252 lab5\r\n", 21);
    write(socket, "Content-type: ", 14);
    write(socket, contentType, strlen(contentType));
    write(socket, "\r\n\r\n", 4);
    write(socket, notFound, strlen(notFound));
}

int sortNameA (const void * name1, const void * name2)
{
    const char * name1_ = *(const char **)name1;
    const char * name2_ = *(const char **)name2;
    return strcmp(name1_, name2_);
}

int sortModifiedTimeA (const void * name1, const void * name2)
{
    const char * name1_ = *(const char **)name1;
    const char * name2_ = *(const char **)name2;
    struct stat name1_stat, name2_stat;
    stat(name1_, &name1_stat);
    stat(name2_, &name2_stat);
    return difftime(name1_stat.st_mtime, name2_stat.st_mtime);

}

int sortSizeA (const void * name1, const void * name2)
{
    const char * name1_ = *(const char **)name1;
    const char * name2_ = *(const char **)name2;
    struct stat name1_stat, name2_stat;
    stat(name1_, &name1_stat);
    stat(name2_, &name2_stat);
    int s1 = name1_stat.st_size;
    int s2 = name2_stat.st_size;
    return s1 - s2;
}

int sortNameD (const void * name2, const void * name1)
{
    const char * name1_ = *(const char **)name1;
    const char * name2_ = *(const char **)name2;
    return strcmp(name1_, name2_);
}

int sortModifiedTimeD (const void * name2, const void * name1)
{
    const char * name1_ = *(const char **)name1;
    const char * name2_ = *(const char **)name2;
    struct stat name1_stat, name2_stat;
    stat(name1_, &name1_stat);
    stat(name2_, &name2_stat);
    return difftime(name1_stat.st_mtime, name2_stat.st_mtime);

}

int sortSizeD (const void * name2, const void * name1)
{
    const char * name1_ = *(const char **)name1;
    const char * name2_ = *(const char **)name2;
    struct stat name1_stat, name2_stat;
    stat(name1_, &name1_stat);
    stat(name2_, &name2_stat);
    return name1_stat.st_size - name2_stat.st_size;
}

