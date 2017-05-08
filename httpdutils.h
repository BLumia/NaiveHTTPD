// Naive HTTPD
// BLumia (c) 2017
// WTFPL

typedef struct supportedFileType {
	char* ext;
	char* type;
} FileType;

typedef struct httpStatusCode {
	int code;
	char* desc;
} StatusCode;

char hex2char(char ch) {
	return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

char *genUrldecodedStr(char *pstr) {
	char *buf = malloc(strlen(pstr) + 1), *pbuf = buf;
	while (*pstr) {
		if (*pstr == '%') {
			if (pstr[1] && pstr[2]) {
				*pbuf++ = hex2char(pstr[1]) << 4 | hex2char(pstr[2]);
				pstr += 2;
			}
		} else if (*pstr == '+') { 
			*pbuf++ = ' ';
		} else {
			*pbuf++ = *pstr;
		}
		pstr++;
	}
	*pbuf = '\0';
	return buf;
}

ssize_t fdgets(int socketfd, char* buffer, int size) {
	
	ssize_t received, i = 0;
	char ch = '\0';
	
	while(i < (size - 1)) {
		if (ch == '\n') break;
		received = recv(socketfd, &ch, 1, 0);
		if (received > 0) {
			if (ch == '\r') { // CR LF -> LF, CR -> LF
				received = recv(socketfd, &ch, 1, MSG_PEEK);
				if (received > 0 && ch == '\n') recv(socketfd, &ch, 1, 0);
				else ch = '\n';
			}
			buffer[i] = ch;
			i++;
		} else {
			break;
		}
	}
	
	buffer[i] = '\0';
	return i;
}
