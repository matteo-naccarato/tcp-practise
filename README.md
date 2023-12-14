# Just three exercises to practise TCP communication
<i>FTP Server / Custom CGI / JSON Server</i>

## FTP SERVER
### Install
``` bash
sudo apt-get install libsqlite3-dev
```
### Compile
``` bash
gcc test.c -o test -lpthread -lsqlite3
```

### Reference
- [RFC 959](https://tools.ietf.org/html/rfc959)
- [Wikipedia](https://en.wikipedia.org/wiki/List_of_FTP_server_return_codes)
