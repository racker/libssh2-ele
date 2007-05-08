/*
 * $Id: sftp_write.c,v 1.3 2007/04/26 23:59:15 gknauf Exp $
 *
 * Sample showing how to do SFTP write transfers.
 *
 * The sample code has default values for host name, user name, password
 * and path to copy, but you can specify them on the command line like:
 *
 * "sftp 192.168.0.1 user password sftp_write.c /tmp/secrets"
 */

#include <libssh2.h>
#include <libssh2_sftp.h>
#include <libssh2_config.h>

#ifdef HAVE_WINSOCK2_H
# include <winsock2.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
# ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

int main(int argc, char *argv[])
{
	unsigned long hostaddr;
	int sock, i, auth_pw = 1;
	struct sockaddr_in sin;
	const char *fingerprint;
	LIBSSH2_SESSION *session;
	char *username=(char *)"username";
	char *password=(char *)"password";
    char *loclfile=(char *)"sftp_write.c";
	char *sftppath=(char *)"/tmp/TEST";
	int rc;
    FILE *local;
	LIBSSH2_SFTP *sftp_session;
	LIBSSH2_SFTP_HANDLE *sftp_handle;
    char mem[1024];
    size_t nread;
    char *ptr;

#ifdef WIN32
	WSADATA wsadata;

	WSAStartup(WINSOCK_VERSION, &wsadata);
#endif

	if (argc > 1) {
		hostaddr = inet_addr(argv[1]);
	} else {
		hostaddr = htonl(0x7F000001);
	}

	if(argc > 2) {
		username = argv[2];
	}
	if(argc > 3) {
		password = argv[3];
	}
	if(argc > 4) {
		loclfile = argv[4];
	}
	if(argc > 5) {
		sftppath = argv[5];
	}
    
    local = fopen(loclfile, "rb");
    if (!local) {
        printf("Can't local file %s\n", loclfile);
        goto shutdown;
    }
    
	/*
	 * The application code is responsible for creating the socket
	 * and establishing the connection
	 */
	sock = socket(AF_INET, SOCK_STREAM, 0);

	sin.sin_family = AF_INET;
	sin.sin_port = htons(22);
	sin.sin_addr.s_addr = hostaddr;
	if (connect(sock, (struct sockaddr*)(&sin), 
		    sizeof(struct sockaddr_in)) != 0) {
		fprintf(stderr, "failed to connect!\n");
		return -1;
	}

	/* Create a session instance
	 */
	session = libssh2_session_init();
	if(!session)
		return -1;

	/* ... start it up. This will trade welcome banners, exchange keys,
	 * and setup crypto, compression, and MAC layers
	 */
	rc = libssh2_session_startup(session, sock);
	if(rc) {
		fprintf(stderr, "Failure establishing SSH session: %d\n", rc);
		return -1;
	}

	/* At this point we havn't yet authenticated.  The first thing to do
	 * is check the hostkey's fingerprint against our known hosts Your app
	 * may have it hard coded, may go to a file, may present it to the
	 * user, that's your call
	 */
	fingerprint = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_MD5);
	printf("Fingerprint: ");
	for(i = 0; i < 16; i++) {
		printf("%02X ", (unsigned char)fingerprint[i]);
	}
	printf("\n");

	if (auth_pw) {
		/* We could authenticate via password */
		if (libssh2_userauth_password(session, username, password)) {
			printf("Authentication by password failed.\n");
			goto shutdown;
		}
	} else {
		/* Or by public key */
		if (libssh2_userauth_publickey_fromfile(session, username,
							"/home/username/.ssh/id_rsa.pub",
							"/home/username/.ssh/id_rsa",
							password)) {
			printf("\tAuthentication by public key failed\n");
			goto shutdown;
		}
	}

	fprintf(stderr, "libssh2_sftp_init()!\n");
	sftp_session = libssh2_sftp_init(session);

	if (!sftp_session) {
		fprintf(stderr, "Unable to init SFTP session\n");
		goto shutdown;
	}

	/* Since we have not set non-blocking, tell libssh2 we are blocking */
	libssh2_sftp_set_blocking(sftp_session, 1);
    
	fprintf(stderr, "libssh2_sftp_open()!\n");
	/* Request a file via SFTP */
	sftp_handle =
        libssh2_sftp_open(sftp_session, sftppath,
                      LIBSSH2_FXF_WRITE|LIBSSH2_FXF_CREAT,
                      LIBSSH2_SFTP_S_IRUSR|LIBSSH2_SFTP_S_IWUSR|
                      LIBSSH2_SFTP_S_IRGRP|LIBSSH2_SFTP_S_IROTH);
    
	if (!sftp_handle) {
		fprintf(stderr, "Unable to open file with SFTP\n");
		goto shutdown;
	}
	fprintf(stderr, "libssh2_sftp_open() is done, now send data!\n");
	do {
        nread = fread(mem, 1, sizeof(mem), local);
        if (nread <= 0) {
            /* end of file */
            break;
        }
        ptr = mem;
        
        do {
            /* write data in a loop until we block */
            rc = libssh2_sftp_write(sftp_handle, ptr, nread);
            ptr += rc;
            nread -= nread;
        } while (rc > 0);
	} while (1);

    fclose(local);
	libssh2_sftp_close(sftp_handle);
	libssh2_sftp_shutdown(sftp_session);

 shutdown:

	libssh2_session_disconnect(session, "Normal Shutdown, Thank you for playing");
	libssh2_session_free(session);

#ifdef WIN32
	Sleep(1000);
	closesocket(sock);
#else
	sleep(1);
	close(sock);
#endif
printf("all done\n");
	return 0;
}