# UDM_08_Chat_TCP_Client_Server
 
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/17)
[![Qt](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)](https://www.qt.io/download-open-source)
![Qt](https://img.shields.io/badge/CMake-3.19+-064F8C?style=flat&logo=cmake&logoColor=white)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://choosealicense.com/licenses/mit/)
 
A client-server chat application over TCP, written in C++.

This project was developed for educational purposes as a course assignment.
 
## Installation
 
Download and install the following tools (CMake, MinGW and Qt Creator are included in the Qt installer):
 
- Qt 6: https://www.qt.io/download-open-source

- Git (optional): https://git-scm.com/downloads (Download the source code as a ZIP file from GitHub, or just clone it with Git)

Then clone the repository, open `Code/CMakeLists.txt` in Qt Creator, select a Qt 6 kit and build both `chattcp1` (Server) and `ChatClient`.

> **Important:** Before upgrading from an earlier version or making changes, back up and reuse your existing Server/Client `AppData` directories (do not delete SQLite or TLS folders) so existing accounts and history are preserved automatically.
 
```bash
git clone https://github.com/thanhhung4412y/UDM_08_Chat_TCP_Client_Server.git
```
 
## Usage
 
Run the server (`chattcp1`) first, then one or more clients (`ChatClient`). The server listens on port `12345`.

To enable email registration and password recovery via Gmail (`smtp.gmail.com:465` over TLS), first turn on 2-Step Verification and create a 16-character [Google App Password](https://myaccount.google.com/apppasswords). Then, run `./Launch-Server-Gmail.ps1` in PowerShell and enter your `chattcp1.exe` path, your Qt `bin` folder (if DLLs aren't deployed yet), your admin Gmail, and the App Password. 

If PowerShell scripts are blocked, you can manually set `CHATTCP_GMAIL_USER` and `CHATTCP_GMAIL_APP_PASSWORD` as environment variables in your terminal session before starting the server.

You can check SMTP connection and authentication status directly on the Server window's **Gmail** line.
  
## Protocol
 
Each packet is one JSON object on a single line, ending with a newline character (UTF-8).
 
Example:
 
```json
{"type":"private","to":"Hung","text":"hello"}
```
 
### Client to Server
 
- `join` (name): log in with a user name
- `chat` (text): send a message to the public room
- `private` (to, text): send a private message to one user
- `reply` (text, quote): reply to a quoted message
- `forward` (to, text): forward a message; empty `to` means the public room
- `sticker` (to, id): send a sticker
- `image` (to, data): send an image; data is a Base64-encoded JPEG

### Server to Client
 
- `joined` (name): login successful
- `system` (text): system notice, example: someone joined or left
- `users` (names): list of online users
- `chat` (sender, text): public room message
- `private` (sender, to, text): private message
- `reply` (sender, text, quote): reply message
- `forward` (sender, to, text): forwarded message
- `sticker` (sender, to, id): sticker
- `image` (sender, to, data): image
- `error` (text): error message

## Features

- Online user list, private messages, reply, forward, edit/recall, custom avatars, emoji, stickers, image sharing, MP4 video sharing (up to 1 GB).

- PBKDF2 password registration/login (8–128 characters), TLS connection, email linking/verification, and Gmail SMTP password recovery (saved in `accounts.sqlite`).

## Limitations

- Chat history is stored on each client device, not synchronized through the server.
- Verification codes (8-digit, 10-minute expiry, max 5 attempts) are rate-limited in RAM (60s cooldown, 5 codes/email/hr, 20 requests/IP/hr, 100 requests/server/hr) for small LAN use behind a firewall; each email binds permanently to one account, and accounts without a linked email must use pre-generated backup codes for recovery.
 
## License
 
[MIT](https://choosealicense.com/licenses/mit/)
